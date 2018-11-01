# DECAF Automation

This python script can help you start DECAF runtime environment by auto finish some command line parameters.

```python
#!/bin/python2
import subprocess, os 
import fcntl 
import time

def input_cmd(p, cmd): 
    if not cmd.endswith("\n"):
        cmd += "\n" 
        p.stdin.write(cmd) 
        p.stdin.write("MARK\n") 
        while True: 
            time.sleep(0.1) 
            try: 
				s = p.stdout.read() 
            except Exception, e:
                continue 
    		print s 
    		if "unknown command: 'MARK'" in s: 
        		break

def main(): # start DECAF 
	decaf_path = raw_input("Enter the root directory of DECAF (i386-softmmu/qemu-system-i386 should be there):") 
    image_path = raw_input("Enter the image path:") 
    os.chdir(decaf_path) 
    p = subprocess.Popen(
        				args="i386-softmmu/qemu-system-i386"
        	 				 "{image_path} -m 512 -monitor stdio"
        						.format(image_path=image_path),
        				stdin=subprocess.PIPE, 
        				stdout=subprocess.PIPE, 
        				shell=True) 
    fl = fcntl.fcntl(p.stdout, fcntl.F_GETFL)
    fcntl.fcntl(p.stdout, fcntl.F_SETFL, fl | os.O_NONBLOCK)
    time.sleep(3) 
    input_cmd(p, "ps") 
    input_cmd(p, "help")

if __name__ == '__main__': 
    main() 
```

