CLASSIC MINI – MITGELIEFERTE ENTWICKLUNGSUMGEBUNG

Enthalten sind:
- Der angepasste PCSX2-Quellcode mit eigenstaendigem Git-Repository.
- Die zugehoerigen Entwicklungsbibliotheken unter pcsx2-source/deps.
- Portable Git-, CMake- und Ninja-Werkzeuge unter tools.
- Das offizielle Microsoft-Offline-Setup fuer C++ Build Tools, ATL und Windows-SDK unter msvc.

AUF EINEM ANDEREN RECHNER EINMAL AUSFUEHREN
1. Die gesamte ZIP entpacken; keine einzelnen Dateien herausziehen.
2. Compiler einrichten.cmd starten und die Microsoft-Setup-Dialoge selbst abschliessen.
   Windows kann Administratorrechte verlangen. Wenn das Setup einen Neustart meldet, zuerst neu starten.
3. Werkzeuge pruefen.cmd starten.
4. Den Launcher starten. Automatische Updates sind in der Entwickler-Ausgabe aktiviert.

Zum normalen Spielen ist diese Compiler-Einrichtung nicht erforderlich.
Der Compiler ist als vollstaendiges Offline-Installationspaket enthalten, nicht als ohne Setup lauffaehiger Einzelordner.
Git, CMake, Ninja, Quellcode und Bibliotheken werden direkt aus diesem Ordner verwendet.
Ein zusaetzlich installiertes Python ist fuer den Updater nicht notwendig.

Der Updater prueft neue Versionen taeglich. Neue Builds erfolgen nach dem Schliessen des Launchers.
Eigene Anpassungen werden uebernommen; bei Konflikten oder fehlgeschlagenen Builds wird die installierte Version beibehalten.
Spiele, BIOS, Memory Cards und Spielstaende werden nicht durch Update-Pakete ersetzt.
Es wird Speicher fuer Quellcode, Build-Zwischenprodukte und Rueckfallsicherungen benoetigt.

Die mitgelieferten Pakete passen zur aktuellen Version. Kuenftige PCSX2-Versionen koennen neue Anforderungen stellen.
Dann versucht der Updater das offizielle Bibliothekspaket nachzuladen; weitere Compiler-/SDK-Aenderungen koennen ein erneutes Setup erfordern.

Offizielle Anleitungen:
https://pcsx2.net/docs/advanced/building/
https://learn.microsoft.com/en-us/visualstudio/install/create-a-network-installation-of-visual-studio

Die Einrichtung kann auch direkt im Launcher unter Systemkonfiguration / Updates mit Compiler einrichten gestartet werden. Dort steht ebenfalls Werkzeuge jetzt pruefen bereit.
