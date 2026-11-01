from pathlib import Path
import sys,zipfile,configparser,io,json,hashlib,subprocess
developer='--developer' in sys.argv
project=Path(__file__).resolve().parent
app=project/'PS2 Mini Classic'
output=(project/'dist')/('PS2 Mini Classic Developer Portable.zip' if developer else 'PS2 Mini Classic Portable.zip')
output.parent.mkdir(parents=True,exist_ok=True)
binary=project/'build/native/pcsx2-qt/PS2 Classic Mini Launcher.exe'
assert binary.exists()
assert b'SELFTEST-PASS.txt' not in binary.read_bytes(), 'Do not package the self-test executable'
ini=configparser.RawConfigParser(strict=False)
ini.optionxform=str
ini.read(app/'PCSX2/inis/PCSX2.ini',encoding='utf-8-sig')
for section in list(ini.sections()):
    if section.startswith(('DEV9/Eth/Hosts','GameList','Debugger','AutoUpdater')):
        ini.remove_section(section)
        continue
    for key,value in list(ini[section].items()):
        if ':\\' in value or ':/' in value or value.startswith('\\\\'):
            ini.remove_option(section,key)
def set_value(section,key,value):
    if not ini.has_section(section):ini.add_section(section)
    ini.set(section,key,str(value))
for key in ('MainWindowGeometry','MainWindowState','DisplayWindowGeometry'):
    ini.remove_option('UI',key)
for section,key,value in [
    ('UI','SetupWizardIncomplete','true'),('UI','StartFullscreen','false'),('UI','FullscreenUITheme','PS2Classic'),
    ('UI','RenderToSeparateWindow','false'),('UI','PauseOnMenu','true'),('UI','Language','de-DE'),
    ('ClassicMini','MusicPath','music/Menu.mp3'),('ClassicMini','MusicEnabled','true'),
    ('ClassicMini','UpdateNoticeAcknowledged','false'),('ClassicMini','ActiveProfile','default'),('ClassicMini','LastBackup','0'),('ClassicMini','IntroEnabled','true'),('ClassicMini','AutomaticBackups','true'),('ClassicMini','MenuSounds','true'),('ClassicMini','AutoCovers','true'),
    ('ClassicMini','AutomaticUpdates','true' if developer else 'false'),('ClassicMini','UpdateChannel','nightly'),
    ('Filenames','BIOS',''),('SPU2/Output','DeviceName',''),('DEV9/Eth','EthEnable','false'),('DEV9/Eth','EthDevice',''),
    ('AutoUpdater','CheckAtStartup','false'),('EmuCore','EnablePINE','false')]:
    set_value(section,key,value)
for key,folder in {'Bios':'bios','MemoryCards':'memcards','Savestates':'sstates','Cheats':'cheats',
                   'Textures':'textures','Covers':'covers','Snapshots':'snaps','Cache':'cache','Logs':'logs',
                   'Patches':'patches','InputProfiles':'inputprofiles','Videos':'videos',
                   'UserResources':'resources','DebuggerLayouts':'debuggerlayouts','DebuggerSettings':'debuggersettings'}.items():
    set_value('Folders',key,folder)
buffer=io.StringIO();ini.write(buffer)
readme='''PS2 MINI CLASSIC â€“ PORTABLE

ZIP vollstaendig entpacken und PS2 Classic Mini Launcher.exe starten.
Keine Installation von Python oder PCSX2 erforderlich. DLLs und Ressourcen neben der EXE belassen.
Beim ersten Start eigenes BIOS und Spieleverzeichnis auswaehlen. Spiele koennen im Ordner Games liegen.
Memory Cards koennen in Systemkonfiguration / Memory Cards eingerichtet werden.

Enthalten: nativer Emulator mit PS2-Oberflaeche, automatische Cover per Seriennummer,
Hintergrundmusik, eigene Scroll-/Bestaetigen-/Zurueck-Sounds und F11 fuer randloses Vollbild.
L3+R3 fuenf Sekunden halten oeffnet das Pausenmenue; Tastatur: Escape.
Im Hauptmenue steht Nach Updates suchen bereit.
Der letzte Reiter unter Systemkonfiguration heisst Updates und zeigt den Einrichtungshinweis.
Beim ersten Start muss der Hinweis einmal bestaetigt werden.

Nicht enthalten: Spiele, BIOS, persoenliche Memory Cards oder Savestates.
Diese ZIP verwendet eine bereinigte Startkonfiguration ohne feste Pfade dieses Rechners.

NEUE FUNKTIONEN
Spielebibliothek mit Zuletzt gespielt, Favoriten, Sammlungen und einer Detailseite.
Spielinfos lassen sich lokal ergaenzen. Unbekannte Angaben werden nicht erfunden.
Benutzerprofile trennen Karten, Savestates, Spieleinstellungen, Favoriten und Spielzeit.
Grafik-Schnellprofile zeigen eine schematische Vorschau und werden pro Spiel gespeichert.
Savestates haben Vorschaubilder und Ueberschreibschutz. Alte States ohne Spielzeit-Metadaten zeigen unbekannt.
Memory Cards lassen sich als Kopien in Ordnerkarten umwandeln und einzelne Spielstaende kopieren.
Die originalen animierten Save-Symbole sind im BIOS-Browser erreichbar; die neue Verwaltung nutzt ein Kartensymbol.
Handbuecher als eigene PDF/PNG/JPG lassen sich im Launcher lesen. Notizen sind pro Benutzer getrennt.
Automatische Sicherungen erfolgen alle sechs Stunden im Menue; sieben automatische Sicherungen bleiben erhalten.
Manuelle Sicherungen werden nicht automatisch geloescht. Vor dem Wiederherstellen wird der aktuelle Stand gesichert.
Die PS2-Startanimation ist ueberspringbar; der Konsolenname ist unter Benutzerprofile aenderbar.
Textur-Werkstatt: pro Spiel oder im Pausenmenue Original-Dumps ansehen und durch eigene Bilder ersetzen.
Die Original-Dumps bleiben erhalten. Vorhandene Ersatzbilder werden gesichert, Aenderungen lassen sich deaktivieren.
Die Textur-Werkstatt ersetzt keinen Bildeditor. Neue Textur-Dumps erfordern einen Hardware-Renderer.

UPDATES
Die manuelle Pruefung fragt die offiziellen PCSX2-Veroeffentlichungen ab.
Die automatische Installation der angepassten Version benoetigt das zusaetzliche
Entwicklungsprojekt (Quellcode, Bibliotheken und Compiler). Dieses gehoert nicht zur ZIP.
Automatische Neubaubuilds sind daher in der portablen Konfiguration abgeschaltet.
Ein unveraendertes PCSX2-Update wird nicht ueber die individuelle Version installiert.

PCSX2: GPL-3.0+. Lizenz liegt in LICENSE-PCSX2.txt, weitere Lizenzen unter licenses.
Quellcodebasis: https://github.com/PCSX2/pcsx2 (v2.9.151, privat angepasst).
'''
temporary=output.with_suffix('.zip.part')
if developer:
    readme=readme.replace('Die automatische Installation der angepassten Version benoetigt das zusaetzliche\nEntwicklungsprojekt (Quellcode, Bibliotheken und Compiler). Dieses gehoert nicht zur ZIP.\nAutomatische Neubaubuilds sind daher in der portablen Konfiguration abgeschaltet.',
        'Diese Entwickler-Ausgabe enthaelt das komplette angepasste Quellcode-Repository, Bibliotheken, Git, CMake, Ninja und das Microsoft-Offline-Setup fuer Compiler/Windows-SDK.\nAuf einem neuen Rechner EINMAL development/Compiler einrichten.cmd starten und das Microsoft-Setup abschliessen. Danach development/Werkzeuge pruefen.cmd starten. Alternativ lassen sich Einrichtung und Werkzeugpruefung direkt unter Systemkonfiguration / Updates starten.\nAutomatische Neubaubuilds sind in dieser Ausgabe aktiviert. Zum Spielen ist das Compiler-Setup nicht erforderlich.\nDer Compiler ist als Offline-Installationspaket enthalten; er muss auf dem jeweiligen Windows-Rechner einmal eingerichtet werden.\nWeitere Hinweise unter development/README-DE.txt.')
entries=[]
with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
    def add(path,relative):
        is_development=relative.parts[0]=='development'
        if not is_development and path.suffix.lower() in ('.pdb','.lib','.exp','.ilk'):return
        if not is_development and path.suffix.lower()=='.dll' and path.stem.endswith('d') and path.with_name(path.stem[:-1]+'.dll').exists():return
        name='PS2 Mini Classic/'+str(relative).replace('\\','/')
        compressed=zipfile.ZIP_STORED if path.suffix.lower() in ('.cab','.msi','.msu','.nupkg','.zip','.vsix','.7z') else zipfile.ZIP_DEFLATED
        archive.write(path,name,compress_type=compressed)
        entries.append(name)
    add(binary,Path(binary.name))
    crt=Path(r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT')
    assert crt.is_dir(),'Redistributable runtime missing'
    for path in crt.glob('*.dll'):add(path,Path(path.name))
    for path in app.glob('*.dll'):add(path,Path(path.name))
    for folder in ('D3D12','QtPlugins','resources','translations','licenses','music','tools'):
        for path in (app/folder).rglob('*'):
            if path.is_file():add(path,path.relative_to(app))
    if developer:
        for path in (app/'development').rglob('*'):
            if path.is_file() and 'compiler-setup' not in path.relative_to(app/'development').parts:
                add(path,path.relative_to(app))
    add(app/'LICENSE-PCSX2.txt',Path('LICENSE-PCSX2.txt'))
    add(app/'PS2 Classic Mini Launcher.ico',Path('PS2 Classic Mini Launcher.ico'))
    add(app/'NEUE-FUNKTIONEN.txt',Path('NEUE-FUNKTIONEN.txt'))
    add(app/'updates/ClassicMiniUpdater.exe',Path('updates/ClassicMiniUpdater.exe'))
    for name in ('classic_updater.py','classic_manual.py','classic_core.py','build_native.ps1','deploy_native.py'):
        add((project/name) if name=='build_native.ps1' else (project/'source'/name),Path('source')/name)
    for path in (app/'development/pcsx2-source/pcsx2/ImGui').glob('ClassicMini*.inl'):
        add(path,Path('source/pcsx2/ImGui')/path.name)
    patch=subprocess.check_output(['git','-C',str(app/'development/pcsx2-source'),'diff','--binary','HEAD'])
    archive.writestr('PS2 Mini Classic/source/ClassicMini-changes.patch',patch)
    archive.writestr('PS2 Mini Classic/source/UPSTREAM.txt',
        'PCSX2 v2.9.151\nCommit: 8d2ad89181db29d2994daaa5f1258f40c303230b\nhttps://github.com/PCSX2/pcsx2\nDie Patchdatei und die ClassicMini*.inl-Dateien enthalten die privaten C++-Anpassungen.\nDie Sounds liegen unter resources/sounds/classic. Die offiziellen Entwicklungsbibliotheken werden zum Bauen benoetigt.\n')
    texts={'qt.conf':'[Paths]\nPlugins = ./QtPlugins\n','portable.txt':'PCSX2\n',
           'README-DE.txt':readme,'PCSX2/inis/PCSX2.ini':buffer.getvalue(),
           'updates/config.json':json.dumps({'channel':'nightly','tool_dirs':[],'git':'git','cmake':'cmake','vcvars':''}),
           'updates/installed.json':json.dumps({'version':'v2.9.151'}),
           'updates/status.txt':'Portable Ausgabe: manuelle Versionspruefung verfuegbar; automatische Neubaubuilds benoetigen das Entwicklungsprojekt.'}
    if developer:
        texts['updates/config.json']=(app/'updates/config.json').read_text(encoding='utf-8')
        texts['updates/status.txt']='Entwickler-Ausgabe: Werkzeuge und Offline-Compiler-Setup enthalten. Auf einem neuen Rechner Compiler einrichten.cmd ausfuehren.'
    for name,text in texts.items():archive.writestr('PS2 Mini Classic/'+name,text.encode('utf-8'))
    for folder in ('Games','PCSX2/bios','PCSX2/memcards','PCSX2/sstates','PCSX2/cheats','PCSX2/textures','PCSX2/covers'):
        archive.writestr('PS2 Mini Classic/'+folder+'/',b'')
with zipfile.ZipFile(temporary) as archive:
    assert archive.testzip() is None
    names=archive.namelist()
    for folder in ('Games/','PCSX2/bios/','PCSX2/memcards/','PCSX2/sstates/'):
        assert not [name for name in names if name.startswith('PS2 Mini Classic/'+folder) and not name.endswith('/')]
    settings=archive.read('PS2 Mini Classic/PCSX2/inis/PCSX2.ini').decode('utf-8')
    assert 'H:\\' not in settings and 'C:\\Users\\' not in settings
    assert 'UpdateNoticeAcknowledged = false' in settings
    assert hashlib.sha256(archive.read('PS2 Mini Classic/'+binary.name)).digest()==hashlib.sha256(binary.read_bytes()).digest()
    assert 'PS2 Mini Classic/msvcp140.dll' in names
    assert 'PS2 Mini Classic/vcruntime140.dll' in names
    if developer:
        for required in ('development/tools/git/cmd/git.exe','development/tools/cmake/bin/cmake.exe',
                         'development/tools/ninja/ninja.exe','development/pcsx2-source/.git/HEAD',
                         'development/pcsx2-source/deps/lib/Qt6Core.lib','development/msvc/vs_setup.exe',
                         'development/Compiler einrichten.cmd'):
            assert 'PS2 Mini Classic/'+required in names,required
        config=archive.read('PS2 Mini Classic/updates/config.json').decode('utf-8')
        assert 'C:\\\\Users' not in config and 'H:\\\\' not in config
    for name in ('scroll.wav','confirm.wav','back.wav'):
        assert 'PS2 Mini Classic/resources/sounds/classic/'+name in names
    assert 'PS2 Mini Classic/tools/ClassicMiniManual.exe' in names
    if developer:
        for required in ('pcsx2/ImGui/ClassicMiniHub.inl','pcsx2/ImGui/ClassicMiniTextures.inl','pcsx2-qt/ClassicMiniServices.cpp','pcsx2-qt/ClassicMiniServices.inl'):
            assert 'PS2 Mini Classic/development/pcsx2-source/'+required in names
    assert not any('/backups/' in n or '/profiles/' in n and '/PCSX2/' in n for n in names)
temporary.replace(output)
print('PASS: portable ZIP integrity, native EXE, menu sounds, no games/BIOS/cards/states, no machine-specific paths')
print(output)
print('ZIP_BYTES',output.stat().st_size)
