"""Deploy the compiled native Classic Mini application without replacing user data."""
from pathlib import Path
import filecmp
import json
import shutil
from classic_core import update_ini

project=Path(__file__).resolve().parent
install=project/'PS2 Mini Classic'
if not install.is_dir():
    install=project
source=project/'pcsx2-source'
binary=project/'build/native/pcsx2-qt/PS2 Classic Mini Launcher.exe'
if not binary.is_file():
    raise SystemExit('Zuerst build_native.ps1 ausfuehren.')

def copy_file(src,dst):
    src,dst=Path(src),Path(dst)
    dst.parent.mkdir(parents=True,exist_ok=True)
    if dst.is_file() and filecmp.cmp(src,dst,shallow=False):
        return str(dst)
    return shutil.copy2(src,dst)

target=install/'PS2 Classic Mini Launcher.exe'
backup=project/'legacy-frontend/PS2 Classic Mini Launcher-python.exe'
if target.exists() and not backup.exists():
    copy_file(target,backup)

for file in (source/'deps/bin').glob('*.dll'):
    copy_file(file,install/file.name)
for name,folder in [('QtPlugins',source/'deps/plugins'),('resources',source/'bin/resources'),
                    ('translations',source/'bin/translations'),('D3D12',source/'deps/bin/D3D12'),
                    ('licenses/native-dependencies',source/'deps/licenses')]:
    shutil.copytree(folder,install/name,dirs_exist_ok=True,copy_function=copy_file)
copy_file(project/'runtime/resources/patches.zip',install/'resources/patches.zip')
copy_file(source/'COPYING.GPLv3',install/'LICENSE-PCSX2.txt')
(install/'qt.conf').write_text('[Paths]\nPlugins = ./QtPlugins\n',encoding='utf-8')
(install/'portable.txt').write_text('PCSX2\n',encoding='utf-8')
cfg=json.loads((install/'config.json').read_text(encoding='utf-8'))
update_ini(install/'PCSX2/inis/PCSX2.ini',[
    ('UI','FullscreenUITheme','PS2Classic'),('UI','FullscreenUIDefaultToGameList','false'),
    ('UI','StartBigPictureMode','true'),('UI','RenderToSeparateWindow','false'),('UI','PauseOnMenu','true'),
    ('ClassicMini','PS2Design','true'),('ClassicMini','AutoCovers','true'),('ClassicMini','MenuSounds','true'),('ClassicMini','MusicEnabled',str(cfg.get('music_enabled',True)).lower()),
    ('ClassicMini','MusicPath',cfg.get('music','')),('Folders','Covers','covers'),
    ('AutoUpdater','CheckAtStartup','false')])

# Transfer launcher-created game profiles where a verified game identity is known.
import hashlib
for game,identity in cfg.get('identities',{}).items():
    import os
    key=hashlib.sha256(os.path.normcase(os.path.abspath(game)).encode()).hexdigest()[:20]
    profile=install/'PCSX2/profiles'/(key+'.ini')
    if profile.exists():
        destination=install/'PCSX2/gamesettings'/(identity[0]+'_'+identity[1]+'.ini')
        if destination.exists():
            copy_file(destination,destination.with_suffix('.ini.before-classic'))
        # Preserve original per-game values, applying only launcher overrides.
        from classic_core import read_ini
        ini=read_ini(profile)
        update_ini(destination,[(section,key,value) for section in ini.sections() for key,value in ini[section].items()])

copy_file(binary,target)
print('Native Anwendung installiert:',target)
print('Python-Rueckfallversion:',backup)
