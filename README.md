# PlayStation 2 Classic Mini Launcher

<img src="PS2%20Mini%20Classic/development/pcsx2-source/bin/resources/icons/AppIconLarge.png" width="240" alt="PS2 Classic Mini Logo">

Ein nativer PS2-Launcher im Classic-Mini-Stil mit integrierter PCSX2-Emulation, animierter Oberfläche und Bedienung über Maus, Tastatur und Controller.

Basis: **PCSX2 v2.9.151**, Commit `8d2ad89181db29d2994daaa5f1258f40c303230b`. Dieses Repository enthält den angepassten Emulator-Quellcode und die Launcher-Werkzeuge. Die Erweiterungen wurden mit Unterstützung von OpenAI Codex entwickelt.

## Download und erster Start

Die fertigen Pakete gibt es unter **[Releases](https://github.com/VenomSnakeTwitch/PlayStation-2-Classic-Emulator/releases)**:

| Paket | Inhalt |
| --- | --- |
| **PS2.Mini.Classic.Portable.zip** | Spielbereiter Launcher, Laufzeitbibliotheken, Sounds und bereinigte Startkonfiguration. |
| **PS2.Mini.Classic.Developer.Portable.zip** | Zusätzlich vollständiger Quellcode, Entwicklungsbibliotheken, Git, CMake, Ninja und Microsoft-Offline-Setup für Compiler/Windows-SDK. |

1. Ein Paket vollständig entpacken und `PS2 Mini Classic/PS2 Classic Mini Launcher.exe` starten.
2. Eigenes BIOS auswählen und eigene Spiele im Ordner `PS2 Mini Classic/Games` ablegen oder ein eigenes Spieleverzeichnis wählen.
3. Den einmaligen Einrichtungshinweis bestätigen. Zum Spielen wird kein Compiler benötigt.

**Beide Pakete enthalten keine Spiele, BIOS-Dateien, persönlichen Memory Cards, Savestates oder Benutzersicherungen.** Prüfsummen stehen in `SHA256SUMS.txt`.

## Funktionen

- PS2-Stil mit animiertem Hintergrund, Hintergrundmusik und Navigationssounds; F11 schaltet randloses Vollbild um.
- Spielebibliothek mit automatischen Covern anhand der Seriennummer, zuletzt gespielten Titeln, Favoriten, Sammlungen, Spielzeit und lokal bearbeitbaren Spielinfos.
- Native globale und spielbezogene PCSX2-Einstellungen, Grafik-Schnellprofile, Netzwerk- und Controller-Einstellungen.
- Benutzerprofile mit getrennten Memory Cards, Savestates, Einstellungen und Favoriten.
- Savestate-Galerie mit Vorschau und Überschreibschutz; Memory-Card-Verwaltung, automatische Sicherungen und Wiederherstellung.
- Textur-Werkstatt im Spielmenü und in der Bibliothek: Dumps ansehen, Ersatzbilder importieren, deaktivieren und zurücksetzen. Das Ersatzbild erhält automatisch den Dateinamen des ausgewählten Dumps.
- Eigene Handbücher und Notizen, RAM-Suche und PNACH-Editor mit zeilenweiser Controller-Eingabe.
- Update-Menü mit manueller Prüfung, Compiler-Einrichtung und Rückkehr zur vorherigen installierten Version.
- Systemkonfiguration → Steam: Launcher über Steams eigenes Hinzufügen-Fenster in die Bibliothek aufnehmen; automatische Steam-Erkennung, Pfadauswahl und kopierbarer EXE-Pfad. Den Eintrag einmal in Steam bestätigen.

## Bedienung

| Aktion | Controller | Tastatur |
| --- | --- | --- |
| Navigieren | Steuerkreuz / linker Stick | Pfeiltasten |
| Bestätigen | A / Kreuz, abhängig von der OK-Belegung | Enter |
| Zurück | B / Kreis, abhängig von der OK-Belegung | Escape |
| Liste / Aktionen wechseln | Links/Rechts oder L1/R1 bzw. LB/RB | Links/Rechts |
| Pausenmenü im Spiel | L3 + R3 fünf Sekunden halten | Escape |
| Übergeordneter Ordner in der Dateiauswahl | X / Quadrat | Rücktaste |
| Randloses Vollbild | konfigurierbare Tastenzuweisung | F11 |

Textdialoge bieten eine Bildschirmtastatur. In der Textur-Werkstatt führt Bestätigen einer Textur zu ihren Aktionen; Zurück führt zunächst zur Liste und danach zum vorherigen Menü.

## Quellcode und Bauen

Das Projekt behält seine portable Ordnerstruktur:

```text
PS2 Mini Classic/development/pcsx2-source/  Angepasster C++-Emulator samt Drittanbieterquellen
PS2 Mini Classic/development/              Compiler-Einrichtung und Werkzeugprüfung
source/                                    Python-Quellen für Updater und Handbuchanzeige
build_native.ps1                           Nativer Windows-Build
make_portable.py                           Bereinigte portable Pakete nach dist/ erstellen
tests/                                     Isolierte Prüfungen für Navigation, Updater und EXE-Symbol
```

Die großen Entwicklungswerkzeuge und Laufzeitdateien werden über die Entwickler-ZIP bereitgestellt. Sie werden nicht zusätzlich in der Git-Historie gespeichert. Details: [Bauanleitung](docs/BUILD.md).

## Prüfstand und Grenzen

Der native Windows-Build und automatisierte Navigationstests mit simulierten Controller-/Tastatureingaben wurden erfolgreich ausgeführt. Geprüft wurden außerdem Update-Rückkehr, der Handbuchleser und die eingebetteten EXE-Symbole. Die allgemeine Menübedienung wurde vom Nutzer mit Controller ausprobiert. Eine vollständige Prüfung aller Controller, Spiele und der Textur-Werkstatt im laufenden Spiel steht noch aus.

Spielinfos werden lokal gepflegt. Grafik-Vorschauen sind schematisch. Originale animierte Memory-Card-Symbole zeigt der BIOS-Browser; die neue Kartenansicht bewahrt die zugehörigen Dateien. Die Textur-Werkstatt übernimmt Zuordnung und Import; zum Zeichnen oder Bearbeiten von Bildern wird ein eigener Bildeditor benötigt. AR-MAX-/Game-Genie-Codes benötigen gegebenenfalls eine Umwandlung in ein unterstütztes Cheat-Format.

Automatische angepasste PCSX2-Updates benötigen die eingerichtete Entwicklungsumgebung. Neue Anforderungen oder Konflikte mit zukünftigen PCSX2-Versionen können eine manuelle Anpassung erfordern; bei fehlgeschlagenen Builds bleibt die installierte Version erhalten.

## Ursprung und Lizenzen

Emulatorbasis: [PCSX2](https://github.com/PCSX2/pcsx2). Die PCSX2-Lizenz GPL-3.0 liegt in `LICENSE` und beim Emulator-Quellcode; Lizenztexte für enthaltene Bibliotheken stehen in den jeweiligen Quellverzeichnissen und unter `PS2 Mini Classic/licenses`. Medien und Drittanbieterdateien behalten ihre jeweiligen Rechte und Lizenzbedingungen. Die Microsoft-Werkzeuge liegen im Entwickler-Paket mit ihren Hersteller-Lizenztexten und Installationsdateien vor.
