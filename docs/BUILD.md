# Windows-Build

## Entwicklungsumgebung bereitstellen

Die Entwickler-ZIP aus den Releases enthält die für diesen Stand verwendeten Compiler-Installationsdateien, Bibliotheken und Werkzeuge. Für einen neuen Rechner zuerst die ZIP in einen eigenen temporären Ordner entpacken. Anschließend im entpackten Ordner `PS2 Mini Classic/development/Compiler einrichten.cmd` starten und das Microsoft-Setup abschließen. `Werkzeuge pruefen.cmd` prüft die Umgebung. Zum bloßen Starten der fertigen EXE ist dieser Schritt nicht erforderlich.

## Checkout vorbereiten

Dieses Repository enthält den vollständigen angepassten Emulator-Quellcode. Die großen Werkzeuge werden aus der entpackten Entwickler-ZIP ergänzt. Folgende Verzeichnisse in die gleichnamigen Verzeichnisse des Checkouts kopieren:

- `PS2 Mini Classic/development/tools`
- `PS2 Mini Classic/development/msvc`
- `PS2 Mini Classic/development/pcsx2-source/deps`
- `PS2 Mini Classic/development/pcsx2-source/.git` — das innere Git-Verzeichnis der PCSX2-Basis; nicht mit dem Git-Verzeichnis im Repository-Hauptordner verwechseln.

Die C++-Quelldateien des Checkouts dabei beibehalten. Das innere Git-Verzeichnis ermöglicht die ursprüngliche Versionskennung und spätere Patch-Übernahme durch den Updater. Es ist im äußeren Repository ignoriert.

## Emulator bauen

Im Hauptordner des Checkouts in PowerShell ausführen:

```powershell
.\build_native.ps1
```

Die fertige EXE liegt danach unter `build/native/pcsx2-qt/PS2 Classic Mini Launcher.exe`. Um sie zu starten, in eine vollständig entpackte portable Anwendung neben deren DLLs, Ressourcen und QtPlugins kopieren. Nur die EXE zu kopieren reicht für eine neue, leere Installation nicht aus.

Optional unterstützt `build_native.ps1 -SelfTest` die isolierten nativen Datentests. Solche Test-Builds sind ausschließlich für das vorgesehene Testverzeichnis bestimmt und werden vom Paket-Skript abgelehnt.

## Python-Helfer

Die ausgelieferten Helfer benötigen kein separat installiertes Python. Ihre Quellen liegen unter `source/`. Für eine eigene Erstellung werden die Versionen in `requirements-build.txt` verwendet:

```powershell
python -m pip install -r requirements-build.txt
python -m PyInstaller --onefile --noconsole --name ClassicMiniUpdater source/classic_updater.py
python -m PyInstaller --onefile --console --collect-all pypdfium2 --name ClassicMiniManual source/classic_manual.py
```

Die Helfer-EXEs gehören anschließend nach `PS2 Mini Classic/updates` beziehungsweise `PS2 Mini Classic/tools`. Bei einer eigenen Veröffentlichung auch die mitgelieferten Drittanbieter-Lizenztexte beibehalten.

`source/deploy_native.py` enthält den früheren Migrationshelfer für die Python-Oberfläche. Der aktuelle native Build verwendet `build_native.ps1` und die oben beschriebene portable Laufzeitumgebung.

## Portable Pakete

Wenn die lokale Anwendung samt Ressourcen und Entwicklungsumgebung vollständig ist:

```powershell
python make_portable.py
python make_portable.py --developer
```

Die ZIPs entstehen unter `dist/`. Das Skript entfernt persönliche Daten und feste Rechnerpfade aus der Startkonfiguration und prüft die Archive auf unzulässige Spiel-, BIOS- und Speicherstanddateien. Vor einer eigenen Veröffentlichung `SHA256SUMS.txt` für die neu erstellten Pakete aktualisieren.

Offizielle Grundlage: [PCSX2-Bauanleitung](https://pcsx2.net/docs/advanced/building/).
