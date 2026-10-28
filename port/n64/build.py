#!/usr/bin/env python3
"""Build a private two-player Dust II ROM using existing libdragon/Tiny3D SDKs."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess
from export_map import export
from export_models import export_models
from bake_viewmodel import bake
from prepare_queue import prepare
ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=pathlib.Path,default=os.environ.get('N64_INST',ROOT.parent/'n64-2048/.build/libdragon'))
p.add_argument('--tiny3d',type=pathlib.Path,default=os.environ.get('TINY3D_DIR',ROOT.parent/'n64-3d-splitscreen/.build/tiny3d'))
p.add_argument('--assets',type=pathlib.Path,default=ROOT/'cops-and-robbers')
p.add_argument('--queue-kib',type=int,choices=[2,4,8,16,32],default=16,help='CPU-to-RSP command buffers; 2 uses the installed SDK unchanged')
p.add_argument('--libdragon-source',type=pathlib.Path,help='Matching SDK source checkout for a private command queue override')
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
if not (a.assets/'cstrike/maps/de_dust2.bsp').is_file():raise SystemExit('Initialize cops-and-robbers with git submodule update --init, or supply --assets /path/to/game-data')
report=export(a.assets/'cstrike/maps/de_dust2.bsp',[a.assets/'cstrike',a.assets/'valve'],out)
models=export_models(a.assets,out)
bake(out)
objects=[];compiled_sources=[]
extra=[]
sdk_source=(a.libdragon_source or sdk.parent/'libdragon-src').resolve()
if a.queue_kib!=2:extra=[prepare(sdk_source,sdk,out/'queue',a.queue_kib)]
for source in [ROOT/'port/n64'/file for file in ['main.c','world.c','game.c','model.c','baked.c','visibility.c','render.c','sound.c','perf.c']]+[out/'map_data.c',out/'model_data.c',out/'sound_data.c',out/'baked_data.c']+extra:
    compiled_sources.append(source)
    obj=out/(source.stem+'.o');objects.append(obj)
    run([sdk/'bin/mips64-elf-gcc','-c',source,'-o',obj,'-march=vr4300','-mtune=vr4300','-mabi=o64','-O2','-g','-std=gnu17','-ffunction-sections','-fdata-sections','-fno-strict-aliasing','-Wall','-Wextra','-Wno-error' if source in extra else '-Werror','-DN64',*(['-DVALIDATE'] if a.validate else []),*(['-DSMOKE_TEST'] if a.smoke_test else []),*(['-DPROFILE'] if a.profile else []),*(['-DBENCHMARK'] if a.benchmark else []),*(['-DPROFILE_SERIAL'] if a.profile_serial else []),*(['-I'+str(sdk_source/'src/rspq'),'-I'+str(sdk_source/'src'),'-I'+str(sdk_source/'include')] if source in extra else []),'-I'+str(sdk/'mips64-elf/include'),'-I'+str(tiny/'src'),'-I'+str(ROOT/'port/n64')])
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
manifest['compiled_sources']={str(s.relative_to(ROOT)):hashlib.sha256(s.read_bytes()).hexdigest() for s in compiled_sources}
manifest['libraries']={str(s):hashlib.sha256(s.read_bytes()).hexdigest() for s in [tiny/'build/libt3d.a',sdk/'mips64-elf/lib/libdragon.a']}
manifest['baked_animation']=json.loads((out/'bake-report.json').read_text())
manifest.update(queue_kib=a.queue_kib,profile=a.profile,benchmark=a.benchmark,serial_diagnostics=a.profile_serial)
(out/(name+'-manifest.json')).write_text(json.dumps(manifest,indent=2)+'\n')
print('Built',rom)
