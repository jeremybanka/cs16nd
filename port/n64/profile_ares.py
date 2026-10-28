#!/usr/bin/env python3
"""Capture one benchmark cycle from ares, then summarize it (macOS/POSIX)."""
import argparse,os,pathlib,selectors,signal,subprocess,sys,time,shutil,hashlib,json
ROOT=pathlib.Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial',action='store_true')
    parser.add_argument('--smoke',action='store_true',help='Capture the moving combat fixture through its acceptance marker')
    parser.add_argument('--validation',action='store_true',help='Capture graphics validation rather than performance; implies --smoke')
    parser.add_argument('--timeout',type=float,default=600)
    parser.add_argument('--output',type=pathlib.Path)
    parser.add_argument('--ares',help='Optional ares executable override')
    args=parser.parse_args()
    if args.serial and (args.smoke or args.validation):parser.error('Serial capture uses the stationary benchmark')
    args.smoke=args.smoke or args.validation
    name='cs64-dust2-2p'+('-smoke-validation' if args.validation else '-smoke-profile' if args.smoke else '-bench'+('-serial' if args.serial else ''))
    rom=ROOT/'build/n64'/(name+'.z64')
    if not rom.exists():parser.error('Build first with '+('--validate --smoke-test' if args.validation else '--profile --smoke-test' if args.smoke else '--benchmark'+(' --profile-serial' if args.serial else '')))
    output=args.output or ROOT/'build/n64'/('validation-final' if args.validation else 'perf-combat' if args.smoke else 'perf-serial' if args.serial else 'perf-report')
    output.parent.mkdir(parents=True,exist_ok=True);log=output.with_suffix('.log')
    # Freeze both files before launch; builds may continue while a capture runs.
    source_manifest=rom.with_name(name+'-manifest.json')
    manifest=output.with_suffix('.manifest.json');snapshot=output.with_suffix('.z64')
    shutil.copy2(source_manifest,manifest);shutil.copy2(rom,snapshot)
    if hashlib.sha256(snapshot.read_bytes()).hexdigest()!=json.loads(manifest.read_text())['sha256']:
        parser.error('ROM does not match its build manifest')
    rom=snapshot
    command=[sys.executable,str(ROOT/'port/n64/run_ares.py'),'--rom',str(rom)]
    base_profile=ROOT/'build/n64/ares-two-player.bml'
    profile=output.with_suffix('.bml')
    if base_profile.exists():
        profile.write_text(base_profile.read_text().replace('Defocus: Pause','Defocus: Block'))
    command+=['--settings-file',str(profile),'--background']
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
                    if line==b'PERF_DONE' or (args.smoke and line.startswith(b'2P SMOKE PASS')):complete=True;break
    finally:
        selector.close()
        try:os.killpg(child.pid,signal.SIGTERM)
        except ProcessLookupError:pass
        try:child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid,signal.SIGKILL);child.wait()
        child.stdout.close()
    if not complete:raise SystemExit(f'Benchmark did not complete; partial log: {log}')
    if args.validation:
        contents=log.read_text(errors='replace')
        if any(word in contents for word in ('RDPQ_VALIDATION','not cache coherent','Assertion failed','ASSERTION FAILED')):raise SystemExit('Validation diagnostics found: '+str(log))
        print('PASS: moving combat fixture, no RDP or cache-coherency diagnostics; '+str(log));return
    subprocess.run([sys.executable,str(ROOT/'port/n64/profile_report.py'),str(log),'--output',str(output),'--manifest',str(manifest)],check=True,cwd=ROOT)
if __name__=='__main__':main()
