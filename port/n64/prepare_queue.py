"""Change only a private SDK object's low-priority command buffer allocation."""
import hashlib,json

def prepare(source,sdk,output,kib):
    if kib not in (2,4,8,16,32):raise ValueError('Unsupported queue size')
    for name in ('rspq.h','rspq_constants.h','rdpq_constants.h'):
        if (source/'include'/name).read_bytes()!=(sdk/'mips64-elf/include'/name).read_bytes():
            raise ValueError('Installed SDK/source mismatch: '+name)
    original=(source/'src/rspq/rspq.c').read_text()
    anchor='rspq_init_context(&lowpri, RSPQ_DRAM_LOWPRI_BUFFER_SIZE);'
    if original.count(anchor)!=1:raise ValueError('Unsupported SDK queue implementation')
    result=original.replace(anchor,f'rspq_init_context(&lowpri, {kib*256}); /* Local allocation only. */')
    output.mkdir(parents=True,exist_ok=True);path=output/'rspq_override.c';path.write_text(result)
    (output/'provenance.json').write_text(json.dumps({'queue_kib':kib,'source_sha256':hashlib.sha256(original.encode()).hexdigest(),'override_sha256':hashlib.sha256(result.encode()).hexdigest()},indent=2)+'\n')
    return path
