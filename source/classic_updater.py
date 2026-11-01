"""Local source-build updater for the private Classic Mini fork. Never installs stock PCSX2."""
from pathlib import Path
import argparse
import contextlib
import ctypes
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

APP_NAME = 'PS2 Classic Mini Launcher.exe'
API = 'https://api.github.com/repos/PCSX2/pcsx2'
VERSION = re.compile(r'^v?(\d+)\.(\d+)\.(\d+)$')
PRIVATE_FILES = ['pcsx2/ImGui/ClassicMiniTextures.inl', 'pcsx2-qt/ClassicMiniServices.cpp', 'pcsx2/ImGui/ClassicMiniHub.inl', 'pcsx2-qt/ClassicMiniServices.inl', 'pcsx2/ImGui/ClassicMini.inl', 'pcsx2/ImGui/ClassicMiniCovers.inl',
                 'pcsx2/ImGui/ClassicMiniSounds.inl', 'pcsx2/ImGui/ClassicMiniTools.inl',
                 'bin/resources/sounds/classic/scroll.wav', 'bin/resources/sounds/classic/confirm.wav',
                 'bin/resources/sounds/classic/back.wav']

def version(tag):
    match = VERSION.fullmatch(tag)
    return tuple(map(int, match.groups())) if match else None

def select_release(releases, installed, channel):
    choices = [r for r in releases if not r.get('draft') and version(r.get('tag_name',''))
               and (channel == 'nightly' or not r.get('prerelease'))]
    choices = [r for r in choices if version(r['tag_name']) > version(installed)]
    return max(choices, key=lambda r: version(r['tag_name']), default=None)

def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding='utf-8')
    os.replace(temporary, path)

def fetch_json(url):
    request = urllib.request.Request(url, headers={'User-Agent':'PS2-Classic-Mini-Updater/1.0',
                                                 'Accept':'application/vnd.github+json'})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)

def validate_relative(name):
    path = Path(name)
    if path.is_absolute() or '..' in path.parts or not path.parts or ':' in name:
        raise ValueError('Ungueltiger Update-Pfad: ' + name)
    if path.parts[0].lower() in ('games','pcsx2','music','updates') or path.name in ('config.json','portable.txt','qt.conf'):
        raise ValueError('Benutzerdaten duerfen nicht aktualisiert werden: ' + name)
    return path

def transactional_install(stage, install, backup, journal, fail_after=None):
    files = sorted(p.relative_to(stage) for p in stage.rglob('*') if p.is_file())
    for relative in files:
        validate_relative(str(relative))
    changes = []
    backup.mkdir(parents=True, exist_ok=True)
    for relative in files:
        target = install / relative
        existed = target.exists()
        if existed:
            saved = backup / relative
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, saved)
        changes.append({'path':str(relative),'existed':existed})
    record = {'state':'applying','install':str(install),'backup':str(backup),'files':changes}
    write_json(journal, record)
    try:
        for index, change in enumerate(changes):
            relative = Path(change['path'])
            target = install / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            temp = target.with_name(target.name + '.classic-update')
            shutil.copy2(stage / relative, temp)
            os.replace(temp, target)
            if fail_after == index:
                raise OSError('Injected update failure')
        record['state'] = 'complete'
        write_json(journal, record)
    except Exception:
        rollback(record)
        record['state'] = 'rolled_back'
        write_json(journal, record)
        raise

def rollback(record):
    install, backup = Path(record['install']), Path(record['backup'])
    for change in record['files']:
        relative = validate_relative(change['path'])
        target = install / relative
        if change['existed']:
            shutil.copy2(backup / relative, target)
        elif target.exists():
            target.unlink()
        temporary = target.with_name(target.name + '.classic-update')
        if temporary.exists():
            temporary.unlink()

def rollback_install(install, home):
    """Restore only the verified program-file set; works after moving the portable folder."""
    ready_path = home/'rollback-ready.json'
    ready = json.loads(ready_path.read_text(encoding='utf-8'))
    backup_id = ready['backup_id']
    if not re.fullmatch(r'[A-Za-z0-9._-]+',backup_id):
        raise ValueError('Ungueltige Update-Sicherung')
    backup = home/'backups'/backup_id
    for change in ready['files']:
        rel = validate_relative(change['path'])
        if change['existed']:
            saved = backup/rel
            if not saved.is_file() or hashlib.sha256(saved.read_bytes()).hexdigest()!=change['sha256']:
                raise ValueError('Vorherige Programmversion fehlt oder ist beschaedigt. Nichts wurde ersetzt.')
    recovery = home/'backups'/('before-rollback-'+str(time.time_ns()))
    stage = home/'rollback-staging'/str(time.time_ns())
    for change in ready['files']:
        rel = validate_relative(change['path'])
        if change['existed']:
            (stage/rel).parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(backup/rel,stage/rel)
    removed=[]
    # First back up files introduced by the newer version so failure can be undone.
    for change in ready['files']:
        rel=validate_relative(change['path'])
        if not change['existed'] and (install/rel).exists():
            dest=recovery/'extra'/rel;dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(install/rel,dest);removed.append(rel)
    try:
        transactional_install(stage,install,recovery,home/'rollback-install-journal.json')
        for rel in removed:(install/rel).unlink()
    except Exception:
        journal=home/'rollback-install-journal.json'
        if journal.exists():rollback(json.loads(journal.read_text()))
        for rel in removed:
            (install/rel).parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(recovery/'extra'/rel,install/rel)
        raise
    write_json(home/'installed.json',ready['previous_state'])
    config_path=home/'config.json'
    config=json.loads(config_path.read_text(encoding='utf-8'))
    blocked=set(config.get('blocked_versions',[]));blocked.add(ready['replaced_version'])
    config['blocked_versions']=sorted(blocked);write_json(config_path,config)
    ready_path.unlink()
    return ready['previous_state']['version']

class Updater:
    def __init__(self, project, install=None):
        self.project = project.resolve()
        self.install = (install or self.project/'PS2 Mini Classic').resolve()
        self.home = self.install/'updates'
        self.config = json.loads((self.home/'config.json').read_text(encoding='utf-8'))
        bundled = self.install/'development'
        if (bundled/'pcsx2-source/CMakeLists.txt').is_file():
            self.project = bundled
        self.source = self.project/'pcsx2-source'
        for key in ('git','cmake'):
            path = Path(self.config[key])
            if not path.is_absolute() and len(path.parts)>1:
                self.config[key] = str(self.install/path)
        self.config['tool_dirs'] = [str(Path(path) if Path(path).is_absolute() else self.install/path)
                                    for path in self.config.get('tool_dirs',[])]
        self.environment = os.environ.copy()
        self.environment['PATH'] = os.pathsep.join(self.config['tool_dirs']) + os.pathsep + self.environment.get('PATH','')
        self.log = self.home/'updater.log'

    def compiler_environment(self):
        configured = self.config.get('vcvars','auto')
        if configured and configured!='auto' and Path(configured).is_file():
            return configured
        installer = Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        if installer.is_file():
            result = subprocess.run([str(installer),'-latest','-products','*','-requires',
                'Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],
                stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
            directory = result.stdout.decode('utf-8-sig').strip()
            candidate = Path(directory)/'VC/Auxiliary/Build/vcvars64.bat'
            if directory and candidate.is_file():
                return str(candidate)
        raise RuntimeError('Compiler/Windows-SDK noch nicht eingerichtet. development\\Compiler einrichten.cmd starten; danach erneut nach Updates suchen.')

    def status(self, message, **extra):
        print(message, flush=True)
        self.home.mkdir(parents=True, exist_ok=True)
        with self.log.open('a',encoding='utf-8') as stream:
            stream.write(time.strftime('%Y-%m-%d %H:%M:%S ') + message + '\n')
        (self.home/'status.txt').write_text(message, encoding='utf-8')
        write_json(self.home/'status.json', {'message':message,'time':time.time(),**extra})

    def run(self, args, cwd=None, output=False):
        flags = subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS if os.name=='nt' else 0
        result = subprocess.run(args, cwd=cwd or self.project, env=self.environment,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=flags)
        if not output or result.returncode:
            with self.log.open('ab') as stream:
                stream.write(result.stdout)
        if result.returncode:
            raise RuntimeError(f'Befehl fehlgeschlagen ({result.returncode}): {Path(str(args[0])).name}; Details in updater.log')
        return result.stdout

    def git(self, *args, source=None, output=False):
        return self.run([self.config['git'],'-C',str(source or self.source),*args],output=output)

    def app_busy(self):
        if os.name != 'nt':
            return False
        kernel = ctypes.windll.kernel32
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(self.install/APP_NAME),0xC0000000,0,None,3,0,None)
        if handle == ctypes.c_void_p(-1).value:
            return True
        kernel.CloseHandle(handle)
        return False

    def wait_for_close(self, message):
        if self.app_busy():
            self.status(message)
        deadline = time.time()+86400
        while self.app_busy():
            if time.time()>deadline:
                raise RuntimeError('Launcher weiterhin geoeffnet. Update wird bei der naechsten Pruefung erneut vorbereitet.')
            time.sleep(5)

    def build(self, source, build, deps):
        compiler = self.compiler_environment()
        ninja = shutil.which('ninja.exe',path=self.environment['PATH'])
        if not ninja:
            raise RuntimeError('Ninja fehlt in der mitgelieferten Werkzeugumgebung.')
        script = build/'build.cmd'
        build.mkdir(parents=True,exist_ok=True)
        # This file contains only local, validated paths and fixed build arguments.
        script.write_text('\n'.join([
            '@echo off','chcp 65001 >nul',f'call "{compiler}" >nul','if errorlevel 1 exit /b 1',
            f'"{self.config["cmake"]}" -S "{source}" -B "{build}" -G Ninja "-DCMAKE_PREFIX_PATH={deps}" "-DGIT_EXECUTABLE={self.config["git"]}" "-DCMAKE_MAKE_PROGRAM={ninja}" -DENABLE_QT_UI=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_NINJA_CMCLDEPS_RC=OFF',
            'if errorlevel 1 exit /b 1',
            f'"{self.config["cmake"]}" --build "{build}" --config Release --target pcsx2-qt --parallel 2',
            'exit /b %errorlevel%','']),encoding='utf-8')
        self.run([os.environ.get('COMSPEC','cmd.exe'),'/d','/c',str(script)])

    def new_dependencies(self, candidate):
        info = fetch_json('https://api.github.com/repos/PCSX2/pcsx2-windows-dependencies/releases/tags/latest-windows-dependencies')
        asset = next(a for a in info['assets'] if a['name']=='pcsx2-windows-dependencies.7z')
        digest = asset.get('digest','')
        if not re.fullmatch(r'sha256:[0-9a-f]{64}',digest):
            raise RuntimeError('Das offizielle Bibliothekspaket besitzt keine pruefbare SHA256-Summe.')
        archive = candidate/'dependencies.7z'
        request = urllib.request.Request(asset['browser_download_url'],headers={'User-Agent':'PS2-Classic-Mini-Updater/1.0'})
        checksum = hashlib.sha256()
        with urllib.request.urlopen(request,timeout=60) as response,archive.open('wb') as stream:
            while chunk := response.read(1024*1024):
                checksum.update(chunk)
                stream.write(chunk)
        if checksum.hexdigest()!=digest.split(':')[1]:
            raise RuntimeError('Bibliotheken-Pruefsumme stimmt nicht.')
        self.run(['tar.exe','-xf',str(archive),'-C',str(candidate)])
        return candidate/'deps'

    def stage(self, source, build, deps, target):
        target.mkdir(parents=True,exist_ok=True)
        exe = build/'pcsx2-qt'/APP_NAME
        if not exe.is_file():
            raise RuntimeError('Der neue Build enthaelt die angepasste Launcher-EXE nicht.')
        shutil.copy2(exe,target/APP_NAME)
        for file in (deps/'bin').glob('*.dll'):
            shutil.copy2(file,target/file.name)
        # Match the app-local C++ runtime to the compiler used for the new build.
        redist_root = Path(self.compiler_environment()).parents[2]/'Redist/MSVC'
        versions = sorted((path for path in redist_root.glob('*') if re.fullmatch(r'\d+(?:\.\d+)+',path.name)),
                          key=lambda path: tuple(map(int,path.name.split('.'))),reverse=True)
        for directory in versions:
            runtime_folders = list((directory/'x64').glob('Microsoft.VC*.CRT'))
            if runtime_folders:
                for file in runtime_folders[0].glob('*.dll'):
                    shutil.copy2(file,target/file.name)
                break
        for name,folder in [('QtPlugins',deps/'plugins'),('resources',source/'bin/resources'),
                            ('translations',source/'bin/translations'),('D3D12',deps/'bin/D3D12'),
                            ('licenses/native-dependencies',deps/'licenses')]:
            shutil.copytree(folder,target/name,dirs_exist_ok=True)
        shutil.copy2(source/'COPYING.GPLv3',target/'LICENSE-PCSX2.txt')
        patches = self.install/'resources/patches.zip'
        if patches.exists():
            shutil.copy2(patches,target/'resources/patches.zip')
        for relative in PRIVATE_FILES:
            if relative.startswith('bin/resources/'):
                installed = self.install/relative.removeprefix('bin/')
                destination = target/relative.removeprefix('bin/')
                if installed.exists():
                    destination.parent.mkdir(parents=True,exist_ok=True)
                    shutil.copy2(installed,destination)

    def check(self, force=False, check_only=False, channel=None):
        state_path = self.home/'installed.json'
        state = json.loads(state_path.read_text(encoding='utf-8'))
        checked = self.home/'last-check.json'
        previous = json.loads(checked.read_text()) if checked.exists() else {}
        if not force and time.time()-previous.get('time',0)<86400 and previous.get('channel') == (channel or self.config.get('channel','nightly')):
            return
        channel = channel or self.config.get('channel','nightly')
        data = fetch_json(API+'/releases/latest' if channel=='stable' else API+'/releases?per_page=30')
        release = select_release(data if isinstance(data,list) else [data],state['version'],channel)
        write_json(checked,{'time':time.time(),'channel':channel})
        if not release:
            self.status(f'Keine neuere {"stabile " if channel=="stable" else ""}PCSX2-Version als {state["version"]} gefunden.')
            return
        tag = release['tag_name']
        if tag in self.config.get('blocked_versions',[]):
            self.status(f'PCSX2 {tag} wurde nach einer Rueckkehr gesperrt. Die aktuelle Version bleibt erhalten.')
            return
        self.status(f'PCSX2 {tag} verfuegbar. Angepasste Version wird vorbereitet.',version=tag)
        if check_only:
            return
        if not (self.source/'CMakeLists.txt').is_file():
            raise RuntimeError('Neue Version gefunden, aber das Entwicklungsprojekt fuer den angepassten Neubau fehlt. Die installierte Version bleibt erhalten.')
        self.compiler_environment()
        origin = self.git('remote','get-url','origin',output=True).decode().strip().rstrip('/')
        if origin not in ('https://github.com/PCSX2/pcsx2','https://github.com/PCSX2/pcsx2.git'):
            raise RuntimeError('Die Quellcode-Quelle ist nicht das offizielle PCSX2-Repository.')
        self.wait_for_close(f'PCSX2 {tag}: automatischer Neubau nach dem Beenden des Launchers.')
        candidate = self.home/'candidates'/tag
        if candidate.exists():
            candidate = candidate.with_name(tag+'-'+str(int(time.time())))
        self.git('fetch','--depth','1','origin','tag',tag)
        self.git('worktree','add','--detach',str(candidate),tag)
        snapshot = self.home/'snapshots'/str(int(time.time()))
        snapshot.mkdir(parents=True)
        patch = self.git('diff','--binary','HEAD',output=True)
        (snapshot/'classic.patch').write_bytes(patch)
        if patch:
            self.git('apply','--3way',str(snapshot/'classic.patch'),source=candidate)
        for relative in PRIVATE_FILES:
            src,dst = self.source/relative,candidate/relative
            if not src.is_file():
                raise RuntimeError('Anpassungsdatei fehlt: '+relative)
            if dst.exists() and dst.read_bytes()!=src.read_bytes():
                raise RuntimeError('Namenskonflikt mit neuer PCSX2-Version: '+relative)
            dst.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(src,dst)
        self.git('diff','--check',source=candidate)
        if shutil.disk_usage(self.project).free < 8*1024**3:
            raise RuntimeError('Fuer den automatischen Neubau werden mindestens 8 GB freier Speicher benoetigt.')
        self.status(f'PCSX2 {tag}: angepasster Emulator wird im Hintergrund gebaut.')
        build = self.home/'builds'/candidate.name
        deps = self.source/'deps'
        try:
            self.build(candidate,build,deps)
        except RuntimeError:
            self.status('Neue Entwicklungsbibliotheken werden geprueft; installierte Version bleibt erhalten.')
            deps = self.new_dependencies(candidate)
            build = build.with_name(build.name+'-new-deps')
            self.build(candidate,build,deps)
        payload = self.home/'staging'/candidate.name
        self.stage(candidate,build,deps,payload)
        self.wait_for_close('Update fertig gebaut. Installation, sobald der Launcher geschlossen ist.')
        backup = self.home/'backups'/f'{state["version"]}-{int(time.time())}'
        transactional_install(payload,self.install,backup,self.home/'install-journal.json')
        record=json.loads((self.home/'install-journal.json').read_text(encoding='utf-8'))
        for change in record['files']:
            if change['existed']:
                change['sha256']=hashlib.sha256((backup/validate_relative(change['path'])).read_bytes()).hexdigest()
        write_json(self.home/'rollback-ready.json',{'backup_id':backup.name,'files':record['files'],
                   'previous_state':state,'replaced_version':tag})
        write_json(state_path,{'version':tag,'commit':self.git('rev-parse','HEAD',source=candidate,output=True).decode().strip(),
                               'installed_at':time.time(),'backup':str(backup)})
        self.status(f'PCSX2 {tag} mit Classic-Mini-Anpassungen installiert. Beim naechsten Start aktiv.')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--project',type=Path,required=True)
    parser.add_argument('--install',type=Path)
    parser.add_argument('--force',action='store_true')
    parser.add_argument('--check-only',action='store_true')
    parser.add_argument('--channel',choices=['stable','nightly'])
    parser.add_argument('--verify-build',action='store_true')
    parser.add_argument('--verify-tools',action='store_true')
    parser.add_argument('--rollback',action='store_true')
    args=parser.parse_args()
    updater=Updater(args.project,args.install)
    updater.home.mkdir(parents=True,exist_ok=True)
    # OS file lock is released automatically even after a crash.
    import msvcrt
    with (updater.home/'worker.lock').open('a+b') as lock:
        lock.seek(0)
        lock.write(b'0');lock.flush();lock.seek(0)
        try:
            msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1)
        except OSError:
            return
        try:
            journal=updater.home/'install-journal.json'
            if journal.exists():
                record=json.loads(journal.read_text(encoding='utf-8'))
                if record['state']=='applying':
                    updater.wait_for_close('Unterbrochenes Update: Wiederherstellung nach dem Schliessen.')
                    rollback(record)
                    record['state']='rolled_back'
                    write_json(journal,record)
            if args.rollback:
                updater.wait_for_close('Vorherige Programmversion wird nach dem Schliessen wiederhergestellt.')
                version=rollback_install(updater.install,updater.home)
                updater.status(f'Vorherige Programmversion {version} wiederhergestellt. Die zurueckgenommene Version bleibt gesperrt.')
                subprocess.Popen([str(updater.install/APP_NAME)],cwd=updater.install)
            elif args.verify_tools:
                updater.compiler_environment()
                updater.git('rev-parse','HEAD',output=True)
                updater.run([updater.config['cmake'],'--version'])
                updater.run(['ninja.exe','--version'])
                updater.status('Compiler erkannt; mitgelieferte Git-, CMake-, Ninja- und Quellcode-Dateien erfolgreich geprueft.')
            elif args.verify_build:
                updater.status('Mitgelieferte Entwicklungsumgebung wird mit einem vollstaendigen Emulator-Build geprueft.')
                updater.build(updater.source,updater.home/'verification-build',updater.source/'deps')
                updater.status('Mitgelieferte Werkzeuge und Bibliotheken: vollstaendiger Emulator-Build erfolgreich.')
            else:
                updater.check(args.force,args.check_only,args.channel)
        except Exception as error:
            updater.status('Automatisches Update nicht installiert: '+str(error),error=True)
            return 1
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(),msvcrt.LK_UNLCK,1)

if __name__=='__main__':
    sys.exit(main() or 0)
