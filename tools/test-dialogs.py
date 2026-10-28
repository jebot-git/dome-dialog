from pathlib import Path
import subprocess,os,time,sys
root=Path(__file__).resolve().parents[1];binary=Path(sys.argv[1]) if len(sys.argv)>1 else root/'build/dome-dialog'
read,write=os.pipe()
x=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write)
with os.fdopen(read) as stream:display=':'+stream.readline().strip()
env={**os.environ,'DISPLAY':display,'FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','XDG_CURRENT_DESKTOP':''}
env.pop('WAYLAND_DISPLAY',None)
def test(name,options,expected,code=0,activate=True,stdin=None):
 p=subprocess.Popen([str(binary),'--title','DOME-DIALOG-TEST',*options],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 try:
  if activate:
   for i in range(100):
    r=subprocess.run(['xdotool','search','--name','^DOME-DIALOG-TEST$'],env=env,capture_output=True,text=True)
    if r.returncode==0:
     window=r.stdout.strip().splitlines()[-1]
     subprocess.run(['xdotool','windowfocus',window],env=env,check=True,capture_output=True)
     time.sleep(.1);subprocess.run(['xdotool','key','Return'],env=env,check=True,capture_output=True);break
    time.sleep(.02)
  out,err=p.communicate(stdin,timeout=5)
  assert (p.returncode,out.strip())==(code,expected),(name,p.returncode,out,err)
  print('PASS',name,flush=True)
 finally:
  if p.poll() is None:p.kill();p.wait()
try:
 test('Message acceptance',['--info','--text','UTF-8 日本語'],'')
 test('Entry text output',['--entry','--entry-text','Žluťoučký 日本語'],'Žluťoučký 日本語')
 test('Radiolist Winetricks row selection',['--list','--radiolist','--column','','--column','','--column','','--hide-column','2','FALSE','help','Help','TRUE','main','Default'],'main')
 test('Checklist multiple return values',['--list','--checklist','--column','','--column','Name','--column','Description','TRUE','one','First','TRUE','two','Second'],'one|two')
 test('Winetricks prefix form',['--forms','--add-combo','Architecture','--combo-values','32|64','--add-entry','Name'],'32|')
 test('Download progress closes',['--progress','--auto-close'],'',activate=False,stdin='0\n# Downloading\n100\n')
 test('Timeout has Zenity exit status',['--question','--timeout','1'],'',code=5,activate=False)
finally:x.terminate();x.wait(timeout=5)
