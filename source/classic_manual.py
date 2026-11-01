"""Render user-provided manuals into cached pages; no external Python is required in the packaged app."""
from pathlib import Path
import argparse,configparser,io,os,shutil,sys
import pypdfium2 as pdfium
from PIL import Image

def atomic(path,data):
 path.parent.mkdir(parents=True,exist_ok=True)
 temp=path.with_name(path.name+'.tmp');temp.write_bytes(data);os.replace(temp,path)
def main():
 p=argparse.ArgumentParser();p.add_argument('--manual',type=Path,required=True);p.add_argument('--cache',type=Path,required=True);p.add_argument('--source',type=Path);p.add_argument('--page',type=int,default=0);a=p.parse_args()
 root=a.manual;root.mkdir(parents=True,exist_ok=True);a.cache.mkdir(parents=True,exist_ok=True)
 if a.source:
  ext=a.source.suffix.lower()
  if ext not in ('.pdf','.png','.jpg','.jpeg'):raise ValueError('Bitte PDF, PNG oder JPG verwenden.')
  if a.source.stat().st_size>512*1024*1024:raise ValueError('Handbuch ist groesser als 512 MB.')
  target=root/('manual.pdf' if ext=='.pdf' else 'manual.png')
  if ext=='.pdf':atomic(target,a.source.read_bytes())
  else:
   with Image.open(a.source) as im:
    im.thumbnail((4096,4096));b=io.BytesIO();im.convert('RGB').save(b,format='PNG');atomic(target,b.getvalue())
  other=root/('manual.png' if ext=='.pdf' else 'manual.pdf')
  if other.exists():other.unlink()
  for file in a.cache.glob('page-*.png'):file.unlink()
 if (root/'manual.pdf').exists():
  with pdfium.PdfDocument(root/'manual.pdf') as doc:
   count=len(doc)
   if not 0<=a.page<count:raise ValueError('Diese Seite existiert nicht.')
   target=a.cache/f'page-{a.page:04}.png'
   if not target.exists():
    page=doc[a.page];scale=min(2.0,1800/max(page.get_width(),page.get_height()));bitmap=page.render(scale=scale)
    b=io.BytesIO();bitmap.to_pil().save(b,format='PNG');atomic(target,b.getvalue());bitmap.close();page.close()
 else:
  count=1
  if not (root/'manual.png').exists():raise ValueError('Kein Handbuch hinterlegt.')
  atomic(a.cache/'page-0000.png',(root/'manual.png').read_bytes())
 atomic(a.cache/'info.ini',f'[Manual]\nPages = {count}\n'.encode())
 return 0
if __name__=='__main__':
 try:sys.exit(main())
 except Exception as e:
  print(str(e));sys.exit(1)
