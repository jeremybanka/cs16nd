#!/usr/bin/env python3
"""Launch two-player CS64 with a project-local ares v148 macOS input profile."""
import argparse,pathlib,re,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--ares',default=str(ROOT/'build/n64/CS64-ares.app/Contents/MacOS/ares') if (ROOT/'build/n64/CS64-ares.app').exists() else '/Applications/ares.app/Contents/MacOS/ares')
p.add_argument('--rom',type=pathlib.Path,default=ROOT/'build/n64/cs64-dust2-2p.z64')
p.add_argument('--reset-profile',action='store_true',help='Regenerate only the project keyboard profile')
p.add_argument('--settings-file',type=pathlib.Path,default=ROOT/'build/n64/ares-two-player.bml')
p.add_argument('--background',action='store_true',help='Keep emulation running but block input when unfocused (isolated capture profiles)')
a=p.parse_args();config=a.settings_file
if not a.rom.is_file():raise SystemExit(f'Build the ROM first: {a.rom}')
if not config.exists() or a.reset_profile:
    default=pathlib.Path.home()/'Library/Application Support/ares/settings.bml'
    text=default.read_text() if default.exists() else ''
    # Quartz key ordering: ares v148 ruby/input/keyboard/quartz.cpp.
    keys={chr(ord('A')+i):40+i for i in range(26)}
    keys.update({'8':29,'9':30,'Delete':34,'Up':92,'Down':93,'Left':94,'Right':95,'Space':98,'Return':97})
    maps=[{'L-Up':'W','L-Down':'S','L-Left':'Left','L-Right':'Right','C-Up':'Up','C-Down':'Down','C-Left':'A','C-Right':'D','A':'Space','B':'E','L':'C','Z':'Q','Start':'Return'},
          {'L-Up':'I','L-Down':'K','L-Left':'U','L-Right':'O','C-Up':'8','C-Down':'9','C-Left':'J','C-Right':'L','A':'N','B':'P','L':'V','Z':'M','Start':'Delete'}]
    section='Nintendo64\n  ExpansionPak: true\n  Input\n'
    for port,bindings in enumerate(maps,1):
        section+=f'    Controller.Port.{port}\n      Gamepad\n'
        for name,key in bindings.items():section+=f'        {name}: 0x1/0/{keys[key]};;\n'
        for axis,lo,hi in [('X-Axis',bindings['L-Left'],bindings['L-Right']),('Y-Axis',bindings['L-Up'],bindings['L-Down'])]:
            section+=f'        {axis}\n          Lo: 0x1/0/{keys[lo]};;\n          Hi: 0x1/0/{keys[hi]};;\n'
    match=re.search(r'(?m)^Nintendo64\n(?:[ \t].*\n|\n)*',text)
    text=text[:match.start()]+section+text[match.end():] if match else text+'\n'+section
    config.parent.mkdir(parents=True,exist_ok=True);config.write_text(text)
if a.background:
    text=config.read_text()
    if re.search(r'(?m)^  Defocus: .*$',text):text=re.sub(r'(?m)^  Defocus: .*$', '  Defocus: Block',text)
    elif re.search(r'(?m)^Input$',text):text=re.sub(r'(?m)^Input$', 'Input\n  Defocus: Block',text,count=1)
    else:text+='\nInput\n  Defocus: Block\n'
    config.write_text(text)
print('P1: WASD / arrows / Space jump / Q fire / E reload / C crouch / Return respawn',flush=True)
print('P2: IJKL / U,O turn / 8,9 look / N jump / M fire / P reload / V crouch / Delete respawn',flush=True)
subprocess.run([a.ares,'--settings-file',str(config),'--system','Nintendo 64','--no-file-prompt',str(a.rom.resolve())],check=True)
