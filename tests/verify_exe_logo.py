from pathlib import Path
import pefile,struct
r=Path(__file__).resolve().parents[1];exe=r/'build/native/pcsx2-qt/PS2 Classic Mini Launcher.exe';ico=r/'PS2 Mini Classic/development/pcsx2-source/pcsx2/windows/AppIcon.ico'
pe=pefile.PE(str(exe));resources={entry.id:entry for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries}
icons={}
for entry in resources[3].directory.entries:
 data=entry.directory.entries[0].data.struct;icons[entry.id]=pe.get_data(data.OffsetToData,data.Size)
raw=ico.read_bytes();count=struct.unpack_from('<H',raw,4)[0];expected=[]
for i in range(count):
 w,h,colors,reserved,planes,bits,size,offset=struct.unpack_from('<BBBBHHII',raw,6+16*i);expected.append(raw[offset:offset+size])
matched=False
for group in resources[14].directory.entries:
 d=group.directory.entries[0].data.struct;data=pe.get_data(d.OffsetToData,d.Size);n=struct.unpack_from('<H',data,4)[0]
 payload=[icons[struct.unpack_from('<H',data,6+14*i+12)[0]] for i in range(n)]
 if payload==expected:matched=True;print('PASS: EXE embeds all',n,'icon sizes byte-for-byte from the supplied artwork')
assert matched
