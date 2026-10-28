#!/usr/bin/env python3
"""Validate PERF JSON lines and write a portable JSON/CSV/Markdown report."""
import argparse,csv,hashlib,json,pathlib

MODEL_STAGES=('player_pose','held_pose','view_pose','bone_matrices','skinning','vertex_writeback','actor_transform')
WORLD_STAGES=('pvs','world0_cull_submit','world1_cull_submit')
def read_log(path):
    meta=None;rows=[]
    for number,line in enumerate(path.read_text(errors='replace').splitlines(),1):
        if line=='PERF_DONE':break
        if line.startswith('PERF_META '):meta=json.loads(line[10:])
        if not line.startswith('PERF '):continue
        try:row=json.loads(line[5:])
        except json.JSONDecodeError as error:raise ValueError(f'{path}:{number}: incomplete PERF record') from error
        assert row['frames']>0 and row['elapsed_ms']>0
        assert abs(row['fps']-row['frames']*1000/row['elapsed_ms'])<.01
        assert all(v>=0 for v in row['cpu_ms'].values())
        assert abs(sum(row['cpu_ms'].values())-row['frame_ms'])<.02,'CPU scopes must partition the measured frame'
        assert row['p50_ms']<=row['p95_ms']<=row['max_ms']
        row['model_cpu_ms']=sum(row['cpu_ms'][k] for k in MODEL_STAGES)
        row['world_cpu_ms']=sum(row['cpu_ms'][k] for k in WORLD_STAGES)
        row['wait_ms']=row['cpu_ms']['display_wait']+row['cpu_ms']['rsp_buffer_wait']
        rows.append(row)
    if not meta or not rows:raise ValueError('No completed profiling windows found')
    return meta,rows

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('build/n64/perf-report'))
    parser.add_argument('--manifest',type=pathlib.Path)
    args=parser.parse_args();meta,rows=read_log(args.log)
    log_text=args.log.read_text(errors='replace')
    diagnostics={'rdp_validation_messages':log_text.count('RDPQ_VALIDATION'),
                 'cache_coherency_messages':log_text.count('not cache coherent'),
                 'assertion_failures':log_text.count('Assertion failed')+log_text.count('ASSERTION FAILED')}
    report={'metadata':meta,'log':str(args.log.resolve()),'log_sha256':hashlib.sha256(args.log.read_bytes()).hexdigest(),'diagnostics':diagnostics,'windows':rows}
    if args.manifest:report['build']=json.loads(args.manifest.read_text())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    flat=[]
    for row in rows:
        flat.append({**{k:v for k,v in row.items() if k not in ('cpu_ms','counts','detail_ms')},**{'cpu_'+k+'_ms':v for k,v in row['cpu_ms'].items()},**{'detail_'+k+'_ms':v for k,v in row.get('detail_ms',{}).items()},**{'count_'+k:v for k,v in row['counts'].items()}})
    with args.output.with_suffix('.csv').open('w',newline='') as file:
        writer=csv.DictWriter(file,fieldnames=list(flat[0]));writer.writeheader();writer.writerows(flat)
    lines=['# CS64 performance capture','',f'Source: `{args.log}`. Serial diagnostic: **{meta["serial"]}**. '
           f'{meta["warmup"]} warm-up frames, then {meta["samples"]} measured frames per window.','',
           '| Scenario | FPS | Frame ms | p95 ms | Model CPU ms | World CPU ms | Wait ms | RDP tail ms | VI flips/s |',
           '| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
    print(f'{"Scenario":25} {"FPS":>6} {"Frame":>7} {"Models":>7} {"World":>7} {"Wait":>7} {"RDPtail":>7}')
    for row in rows:
        keys=('fps','frame_ms','p95_ms','model_cpu_ms','world_cpu_ms','wait_ms','rdp_queue_tail_ms','vi_buffer_changes_hz')
        lines.append('| '+row['scenario']+' | '+' | '.join(f'{row[k]:.2f}' for k in keys)+' |')
        print(f'{row["scenario"]:25} '+ ' '.join(f'{row[k]:7.2f}' for k in ('fps','frame_ms','model_cpu_ms','world_cpu_ms','wait_ms','rdp_queue_tail_ms')))
    lines+=['','CPU scope means partition the frame; they include interrupts and implicit queue backpressure. '
            'Audio ISR time is an overlapping observation, not an additional cost to add. '
            'RDP frame latency runs from graphics submission start to the completion interrupt; '
            'RDP tail runs from final detach submission to completion. Both include queued work and overlap CPU work. '
            'Neither is pure GPU execution time. VI flips count observed framebuffer-origin changes, not just refreshes.','',
            'Serial drains deliberately destroy CPU/RSP/RDP overlap and are diagnostic only. '
            'Do not subtract serial pass times from pipelined frame times. '
            'These are emulated CPU Count measurements, not host wall time or real-console validation.','']
    first=next((r for r in rows if r['scenario']=='two_full'),rows[0])
    lines+=['## CPU breakdown: '+first['scenario'],'','| Scope | Mean ms/frame | Share |','| --- | ---: | ---: |']
    for key,value in sorted(first['cpu_ms'].items(),key=lambda kv:-kv[1]):
        lines.append(f'| {key} | {value:.4f} | {value/first["frame_ms"]:.1%} |')
    lines+=['',f'Overlapping audio callback: {first["audio_isr_ms"]:.4f} ms/frame.','']
    if first.get('audio_callbacks')==0:
        lines+=['**No audio callbacks were observed.** Zero audio time does not demonstrate a cheap audio mixer; the audio path was inactive.','']
    if any(diagnostics.values()):
        lines+=['## Capture diagnostics','',json.dumps(diagnostics), '',
                'Inspect the original log before treating this as a clean hardware-validation run. Profiling is separate from graphics validation.','']
    if 'detail_ms' in first:
        lines+=['## Nested skinning details','','These overlap the CPU `skinning` scope; do not add them to the frame total.','',
                '| Detail | Mean ms/frame |','| --- | ---: |']
        lines += [f'| {key} | {value:.4f} |' for key,value in first['detail_ms'].items()]
    args.output.with_suffix('.md').write_text('\n'.join(lines))
    print('Wrote',args.output.with_suffix('.json'),args.output.with_suffix('.csv'),args.output.with_suffix('.md'))
if __name__=='__main__':main()
