#!/usr/bin/env python3
"""Capture one benchmark cycle from ares, then summarize it (macOS/POSIX)."""
import argparse,os,pathlib,selectors,signal,subprocess,sys,time
ROOT=pathlib.Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial',action='store_true')
    parser.add_argument('--timeout',type=float,default=600)
    parser.add_argument('--output',type=pathlib.Path)
    parser.add_argument('--ares',help='Optional ares executable override')
    args=parser.parse_args()
    name='cs64-dust2-2p-bench'+('-serial' if args.serial else '')
    rom=ROOT/'build/n64'/(name+'.z64')
    if not rom.exists():parser.error('Build first with --benchmark'+(' --profile-serial' if args.serial else ''))
    output=args.output or ROOT/'build/n64'/('perf-serial' if args.serial else 'perf-report')
    output.parent.mkdir(parents=True,exist_ok=True);log=output.with_suffix('.log')
    command=[sys.executable,str(ROOT/'port/n64/run_ares.py'),'--rom',str(rom)]
    if args.ares:command+=['--ares',args.ares]
    # A new process group lets cleanup stop only this wrapper and its emulator.
    child=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
    selector=selectors.DefaultSelector();selector.register(child.stdout,selectors.EVENT_READ)
    deadline=time.monotonic()+args.timeout;pending=b'';complete=False
    try:
        with log.open('wb') as file:
            while time.monotonic()<deadline and not complete:
                ready=selector.select(.5)
                if not ready:
                    if child.poll() is not None:break
                    continue
                chunk=os.read(child.stdout.fileno(),65536)
                if not chunk:break
                file.write(chunk);file.flush();pending+=chunk
                while b'\n' in pending:
                    line,pending=pending.split(b'\n',1)
                    if line.startswith(b'PERF_BEGIN '):print(line.decode(),flush=True)
                    if line==b'PERF_DONE':complete=True;break
    finally:
        selector.close()
        try:os.killpg(child.pid,signal.SIGTERM)
        except ProcessLookupError:pass
        try:child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid,signal.SIGKILL);child.wait()
        child.stdout.close()
    if not complete:raise SystemExit(f'Benchmark did not complete; partial log: {log}')
    subprocess.run([sys.executable,str(ROOT/'port/n64/profile_report.py'),str(log),'--output',str(output),'--manifest',str(rom.with_name(name+'-manifest.json'))],check=True,cwd=ROOT)
if __name__=='__main__':main()
