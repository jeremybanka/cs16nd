#!/usr/bin/env python3
"""Build a private two-player Dust II ROM using existing libdragon/Tiny3D SDKs."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess
from export_map import export
from export_models import export_models
ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=pathlib.Path,default=os.environ.get('N64_INST',ROOT.parent/'n64-2048/.build/libdragon'))
p.add_argument('--tiny3d',type=pathlib.Path,default=os.environ.get('TINY3D_DIR',ROOT.parent/'n64-3d-splitscreen/.build/tiny3d'))
p.add_argument('--assets',type=pathlib.Path,default=ROOT/'cops-and-robbers')
p.add_argument('--validate',action='store_true')
p.add_argument('--smoke-test',action='store_true')
p.add_argument('--profile',action='store_true',help='CPU scopes, async RDP completion and VI observations; no graphics validator')
p.add_argument('--benchmark',action='store_true',help='Cycle controlled T-spawn ablations; implies --profile')
p.add_argument('--profile-serial',action='store_true',help='Diagnostic per-pass RSP/RDP drains; disrupts overlap, implies --profile')
a=p.parse_args();out=ROOT/'build/n64';out.mkdir(parents=True,exist_ok=True)
a.profile=a.profile or a.benchmark or a.profile_serial
if a.profile and a.validate:p.error('Performance profiling and the slow graphics validator must be separate builds')
if a.benchmark and a.smoke_test:p.error('Use either the stationary benchmark or the scripted movement route')
sdk=a.sdk.resolve();tiny=a.tiny3d.resolve()
def run(args):subprocess.run([str(x) for x in args],check=True,cwd=ROOT,env={**os.environ,"N64_INST":str(sdk)})
if not (sdk/'bin/mips64-elf-gcc').exists() or not (tiny/'build/libt3d.a').exists():raise SystemExit('Supply built libdragon --sdk and --tiny3d paths')
report=export(a.assets/'cstrike/maps/de_dust2.bsp',[a.assets/'cstrike',a.assets/'valve'],out)
models=export_models(a.assets,out)
objects=[]
for source in [ROOT/'port/n64'/file for file in ['main.c','world.c','game.c','model.c','render.c','sound.c','perf.c']]+[out/'map_data.c',out/'model_data.c',out/'sound_data.c']:
    obj=out/(source.stem+'.o');objects.append(obj)
    run([sdk/'bin/mips64-elf-gcc','-c',source,'-o',obj,'-march=vr4300','-mtune=vr4300','-mabi=o64','-O2','-g','-std=gnu17','-ffunction-sections','-fdata-sections','-fno-strict-aliasing','-Wall','-Wextra','-Werror','-DN64',*(['-DVALIDATE'] if a.validate else []),*(['-DSMOKE_TEST'] if a.smoke_test else []),*(['-DPROFILE'] if a.profile else []),*(['-DBENCHMARK'] if a.benchmark else []),*(['-DPROFILE_SERIAL'] if a.profile_serial else []),'-I'+str(sdk/'mips64-elf/include'),'-I'+str(tiny/'src'),'-I'+str(ROOT/'port/n64')])
name='cs64-dust2-2p'+('-smoke' if a.smoke_test else '')+('-validation' if a.validate else '')+('-bench' if a.benchmark else '-profile' if a.profile else '')+('-serial' if a.profile_serial else '')
elf=out/(name+'.elf')
run([sdk/'bin/mips64-elf-g++','-o',elf,*objects,tiny/'build/libt3d.a','-lc','-mabi=o64','-Wl,-L'+str(sdk/'mips64-elf/lib'),'-Wl,-ldragon','-Wl,-lm','-Wl,-ldragonsys','-Wl,-Tn64.ld','-Wl,--gc-sections','-Wl,--wrap,__do_global_ctors','-Wl,-Map='+str(elf.with_suffix('.map'))])
run([sdk/'bin/mips64-elf-size',elf])
sym=elf.with_suffix('.sym');run([sdk/'bin/n64sym',elf,sym])
stripped=elf.with_suffix('.stripped');shutil.copy2(elf,stripped)
run([sdk/'bin/mips64-elf-strip','-s',stripped]);run([sdk/'bin/n64elfcompress','-o',out,'-c','1',stripped])
rom=elf.with_suffix('.z64')
run([sdk/'bin/n64tool','--title','CS64 DUST II 2P','--toc','--output',rom,'--align','256',stripped,'--align','8',sym])
run([sdk/'bin/ed64romconfig','--savetype','none','--regionfree',rom])
manifest={'rom':str(rom),'sha256':hashlib.sha256(rom.read_bytes()).hexdigest(),'bytes':rom.stat().st_size,'bsp_sha256':report['bsp_sha256'],'models':{name:info['sha256'] for name,info in models.items()},'players':2,'ram_required_mib':8,'validation':a.validate,'scripted_movement':a.smoke_test,'sdk':str(sdk),'tiny3d':str(tiny),'sources':{str(s.relative_to(ROOT)):hashlib.sha256(s.read_bytes()).hexdigest() for s in (ROOT/'port/n64').glob('*') if s.suffix in ('.c','.h','.py')}}
manifest.update(profile=a.profile,benchmark=a.benchmark,serial_diagnostics=a.profile_serial)
(out/(name+'-manifest.json')).write_text(json.dumps(manifest,indent=2)+'\n')
print('Built',rom)
