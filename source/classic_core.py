"""Storage and PINE integration. No GUI imports, so protocols can be tested independently."""
from pathlib import Path
import configparser
import json
import os
import re
import socket
import struct
import tempfile
import time


def atomic_write(path, text):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix=path.name, suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8', newline='\n') as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def read_ini(path):
    ini = configparser.RawConfigParser(strict=False, empty_lines_in_values=False)
    ini.optionxform = str
    if Path(path).exists():
        ini.read(path, encoding='utf-8-sig')
    return ini


def update_ini(path, changes):
    """Preserve unrelated lines, comments and PCSX2's repeated list entries."""
    path = Path(path)
    original = path.read_text(encoding='utf-8-sig') if path.exists() else ''
    lines = original.splitlines()
    for section, key, value in changes:
        start = next((i for i, line in enumerate(lines) if line.strip() == f'[{section}]'), None)
        if start is None:
            lines.extend(['', f'[{section}]'])
            start = len(lines) - 1
        end = next((i for i in range(start + 1, len(lines)) if lines[i].lstrip().startswith('[')), len(lines))
        matches = [i for i in range(start + 1, end) if lines[i].split('=', 1)[0].strip() == key]
        if value is None:
            for i in reversed(matches):
                del lines[i]
        elif matches:
            lines[matches[0]] = f'{key} = {value}'
            for i in reversed(matches[1:]):
                del lines[i]
        else:
            lines.insert(end, f'{key} = {value}')
    if path.exists():
        backup = path.with_name(path.name + '.' + time.strftime('%Y%m%d-%H%M%S') + '.bak')
        if not backup.exists():
            backup.write_text(original, encoding='utf-8')
    atomic_write(path, '\n'.join(lines) + '\n')


class Pine:
    """PCSX2 PINE, little endian framed TCP on Windows, loopback only."""
    def __init__(self, port=28021):
        self.port = port

    @staticmethod
    def receive(sock, count):
        chunks = bytearray()
        while len(chunks) < count:
            part = sock.recv(count - len(chunks))
            if not part:
                raise ConnectionError('PCSX2 hat die Verbindung geschlossen')
            chunks.extend(part)
        return bytes(chunks)

    def request(self, body):
        with socket.create_connection(('127.0.0.1', self.port), timeout=3) as sock:
            sock.settimeout(5)
            sock.sendall(struct.pack('<I', len(body) + 4) + body)
            size, = struct.unpack('<I', self.receive(sock, 4))
            if not 5 <= size <= 1024 * 1024:
                raise ValueError('Ungueltige PINE-Antwort')
            reply = self.receive(sock, size - 4)
        if reply[0] != 0:
            raise RuntimeError('PCSX2 hat die Anfrage abgelehnt. Ist ein Spiel gestartet?')
        return reply[1:]

    def text(self, opcode):
        data = self.request(bytes([opcode]))
        if len(data) < 4:
            raise ValueError('PINE-Text unvollstaendig')
        length, = struct.unpack_from('<I', data)
        if length > len(data) - 4:
            raise ValueError('PINE-Text unvollstaendig')
        return data[4:4 + length].rstrip(b'\0').decode('utf-8')

    def identity(self):
        return self.text(12), self.text(13).upper()

    def state(self, slot, save=False):
        if not 1 <= slot <= 10:
            raise ValueError('Slot muss zwischen 1 und 10 liegen')
        self.request(bytes([9 if save else 10, slot]))

    def values(self, addresses, width):
        if width not in (1, 2, 4, 8):
            raise ValueError('Ungueltige Datenbreite')
        op = {1: 0, 2: 1, 4: 2, 8: 3}[width]
        result = []
        for offset in range(0, len(addresses), 2048):
            batch = addresses[offset:offset + 2048]
            if any(a < 0 or a + width > 0x2000000 for a in batch):
                raise ValueError('Adresse ausserhalb des 32-MB-PS2-RAMs')
            reply = self.request(b''.join(bytes([op]) + struct.pack('<I', a) for a in batch))
            if len(reply) != len(batch) * width:
                raise ValueError('PINE-Speicherantwort unvollstaendig')
            result.extend(int.from_bytes(reply[i:i + width], 'little') for i in range(0, len(reply), width))
        return result

    def write(self, address, width, value):
        if width not in (1, 2, 4) or address < 0 or address + width > 0x2000000:
            raise ValueError('Ungueltige RAM-Adresse oder Datenbreite')
        self.request(bytes([{1: 4, 2: 5, 4: 6}[width]]) + struct.pack('<I', address) + value.to_bytes(width, 'little'))


class MemorySearch:
    def __init__(self, pine):
        self.pine = pine
        self.candidates = {}
        self.identity = None
        self.width = None

    def scan(self, start, end, width, mode, value, cancel, progress):
        identity = self.pine.identity()
        if not 0 <= start < end <= 0x2000000 or width not in (1, 2, 4):
            raise ValueError('Suchbereich: 00000000 bis 02000000, Breite: 1/2/4 Byte')
        if self.candidates and (self.identity != identity or self.width != width):
            raise ValueError('Spiel oder Datenbreite geaendert: neue Suche starten')
        if mode != 'exact' and not self.candidates:
            raise ValueError('Zuerst eine Suche nach einem bekannten Wert starten')
        addresses = list(self.candidates) if self.candidates else range(start, end - width + 1, width)
        found = {}
        for offset in range(0, len(addresses), 2048):
            if cancel.is_set():
                raise RuntimeError('Suche abgebrochen; bisherige Treffer bleiben erhalten')
            batch = addresses[offset:offset + 2048]
            current = self.pine.values(batch, width)
            for address, val in zip(batch, current):
                old = self.candidates.get(address)
                match = ((mode == 'exact' and val == value) or
                         (mode == 'changed' and val != old) or
                         (mode == 'unchanged' and val == old) or
                         (mode == 'increased' and val > old) or
                         (mode == 'decreased' and val < old))
                if match:
                    found[address] = val
            if len(found) > 250000:
                raise ValueError('Zu viele Treffer; bitte einen kleineren Bereich oder selteneren Wert suchen')
            progress(min(1, (offset + len(batch)) / max(1, len(addresses))))
        if self.pine.identity() != identity:
            raise RuntimeError('Spiel wurde waehrend der Suche gewechselt')
        self.candidates, self.identity, self.width = found, identity, width
        return found


PATCH_LINE = re.compile(r'^patch=([01]),(EE|IOP),([0-9a-fA-F]{1,8}),(byte|short|word|double|extended),([0-9a-fA-F]{1,16})$', re.I)


def validate_pnach(text):
    count = 0
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith(('//', '#', 'gametitle=', 'comment=', '[')):
            continue
        match = PATCH_LINE.fullmatch(line)
        if not match:
            raise ValueError(f'Zeile {number}: PNACH erwartet; verschluesselte AR/MAX/Game-Genie-Codes zuerst konvertieren')
        kind, value = match[4].lower(), match[5]
        limit = {'byte': 2, 'short': 4, 'word': 8, 'double': 16, 'extended': 8}[kind]
        if len(value) > limit:
            raise ValueError(f'Zeile {number}: Wert zu gross fuer {kind}')
        count += 1
    if count == 0:
        raise ValueError('Keine PNACH-Patchzeilen gefunden')
    return count
