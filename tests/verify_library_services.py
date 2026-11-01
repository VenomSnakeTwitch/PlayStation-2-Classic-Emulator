from pathlib import Path
import importlib.util,tempfile,json,hashlib,subprocess
from PIL import Image
r=Path(__file__).resolve().parents[1];t=r/'tests';sp=importlib.util.spec_from_file_location('u',r/'source/classic_updater.py');u=importlib.util.module_from_spec(sp);sp.loader.exec_module(u)
with tempfile.TemporaryDirectory(dir=t) as d:
 p=Path(d).resolve();assert p.is_relative_to(t.resolve());app=p/'renamed portable';h=app/'updates';h.mkdir(parents=True)
 (app/u.APP_NAME).write_bytes(b'old');(app/'PCSX2/memcards').mkdir(parents=True);(app/'PCSX2/memcards/user.ps2').write_bytes(b'private')
 s=p/'stage';s.mkdir();(s/u.APP_NAME).write_bytes(b'new');(s/'new.dll').write_bytes(b'dll');b=h/'backups/v2.0.0-123';j=h/'install-journal.json';u.transactional_install(s,app,b,j)
 rec=json.loads(j.read_text());files=rec['files']
 for c in files:
  if c['existed']:c['sha256']=hashlib.sha256((b/c['path']).read_bytes()).hexdigest()
 u.write_json(h/'rollback-ready.json',{'backup_id':b.name,'files':files,'previous_state':{'version':'v2.0.0'},'replaced_version':'v2.0.1'});u.write_json(h/'config.json',{})
 (b/u.APP_NAME).write_bytes(b'broken')
 try:u.rollback_install(app,h);raise AssertionError('corruption accepted')
 except ValueError:pass
 assert (app/u.APP_NAME).read_bytes()==b'new';(b/u.APP_NAME).write_bytes(b'old');assert u.rollback_install(app,h)=='v2.0.0'
 assert (app/u.APP_NAME).read_bytes()==b'old' and not (app/'new.dll').exists() and (app/'PCSX2/memcards/user.ps2').read_bytes()==b'private'
 assert json.loads((h/'config.json').read_text())['blocked_versions']==['v2.0.1']
 for name in ['../outside','Games/test.iso','PCSX2/memcards/user.ps2','C:/external']:
  try:u.validate_relative(name);raise AssertionError(name)
  except ValueError:pass
 print('PASS: update rollback after relocation, corrupted backup rejected, user card retained, bad version blocked.')
 image=Image.new('RGB',(300,200),(210,30,40));pdf=p/'Handbuch Test.pdf';image.save(pdf,format='PDF');cache=p/'cache';manual=p/'manual'
 helper=r/'PS2 Mini Classic/tools/ClassicMiniManual.exe'
 result=subprocess.run([str(helper),'--manual',str(manual),'--cache',str(cache),'--source',str(pdf),'--page','0'],capture_output=True)
 assert result.returncode==0,result.stdout+result.stderr
 with Image.open(cache/'page-0000.png') as rendered:
  color=rendered.convert('RGB').getpixel((rendered.width//2,rendered.height//2));assert color[0]>180 and color[1]<60,color
 assert 'Pages = 1' in (cache/'info.ini').read_text()
 print('PASS: packaged manual reader rendered PDF correctly without external Python.')
