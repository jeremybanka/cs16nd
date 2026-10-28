#!/usr/bin/env python3
"""Run host collision, gameplay, skinning, and source animation fidelity checks."""
import argparse,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2];OUT=ROOT/'build/n64'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--assets',type=pathlib.Path,default=ROOT/'cops-and-robbers')
a=p.parse_args()
if not (OUT/'model_data.c').exists():raise SystemExit('Run port/n64/build.py first.')
for name,files in [('world',['test_world.c','world.c']),('game',['test_game.c','game.c','model.c','world.c']),('baked',['test_baked.c','model.c','baked.c']),('visibility',['test_visibility.c','visibility.c','game.c','model.c','world.c'])]:
    generated=[OUT/'map_data.c']+([OUT/'model_data.c'] if name!='world' else [])+([OUT/'baked_data.c'] if name=='baked' else [])
    exe=OUT/('test-'+name)
    subprocess.run(['cc','-std=c17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'port/n64'),*(str(ROOT/'port/n64'/f) for f in files),*map(str,generated),'-lm','-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
    (OUT/('test-'+name+'.log')).write_text(result.stdout+result.stderr)
    print(result.stdout.splitlines()[-1])
result=subprocess.run(['python3',str(ROOT/'port/n64/test_model_reference.py'),'--assets',str(a.assets)],check=True,capture_output=True,text=True)
(OUT/'test-model-reference.log').write_text(result.stdout+result.stderr);print(result.stdout.strip())
subprocess.run(['python3',str(ROOT/'port/n64/test_profile_report.py')],check=True)
