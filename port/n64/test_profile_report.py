"""Check that the report rejects misleading or incomplete timing records."""
import json,pathlib,tempfile,unittest
from profile_report import read_log,MODEL_STAGES,WORLD_STAGES

class ReportTests(unittest.TestCase):
    def record(self):
        stages={k:0 for k in (*MODEL_STAGES,*WORLD_STAGES,'display_wait','rsp_buffer_wait')}
        stages['skinning']=10;stages['other']=10
        return {'scenario':'two_full','frames':120,'elapsed_ms':2400,'fps':50,'frame_ms':20,
                'p50_ms':19,'p95_ms':21,'max_ms':22,'cpu_ms':stages,'counts':{}}
    def parse(self,text):
        with tempfile.TemporaryDirectory() as folder:
            path=pathlib.Path(folder)/'capture.log';path.write_text(text);return read_log(path)
    def text(self,record):return 'PERF_META {"version":1}\nPERF '+json.dumps(record)+'\n'
    def test_first_complete_cycle(self):
        meta,rows=self.parse(self.text(self.record())+'PERF_DONE\nPERF {truncated next cycle')
        self.assertEqual(meta['version'],1);self.assertEqual(len(rows),1)
        self.assertEqual(rows[0]['model_cpu_ms'],10)
    def test_overlapping_scopes_are_rejected(self):
        row=self.record();row['cpu_ms']['other']=20
        with self.assertRaises(AssertionError):self.parse(self.text(row))
    def test_wrong_fps_is_rejected(self):
        row=self.record();row['fps']=60
        with self.assertRaises(AssertionError):self.parse(self.text(row))
    def test_incomplete_record_is_rejected(self):
        with self.assertRaises(ValueError):self.parse('PERF_META {}\nPERF {"scenario":')
    def test_no_measured_frames_is_rejected(self):
        with self.assertRaises(ValueError):self.parse('PERF_META {"version":1}\nPERF_BEGIN two_full\n')
if __name__=='__main__':unittest.main()
