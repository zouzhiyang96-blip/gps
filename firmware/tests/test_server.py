from pathlib import Path
import importlib.util,socket,sqlite3,threading,unittest,uuid

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('server_under_test',ROOT/'tools'/'test_server.py')
s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
ID='0123456789ABCDEF01234567'
FRAME=f'DATA,{ID},1,3600,1,1\n'.encode()

class Tests(unittest.TestCase):
    def setUp(self):
        self.path=ROOT/'build'/'server-tests'/(uuid.uuid4().hex+'.sqlite3')
        self.store=s.Store(self.path)
    def tearDown(self):self.store.close()
    def test_parse(self):self.assertEqual(s.parse_data(FRAME),(ID,1,3600,1,1))
    def test_invalid_frames(self):
        for frame in [FRAME+b'junk',FRAME.replace(b',1,3600',b',0,3600'),FRAME.replace(b',3600,',b',99999,'),b'X'*161,b'DATA,wrong,1,3600,1,1\n']:
            with self.subTest(frame=frame),self.assertRaises(ValueError):s.parse_data(frame)
    def test_duplicate_is_idempotent(self):
        record=s.parse_data(FRAME)
        self.assertTrue(self.store.persist(record));self.assertFalse(self.store.persist(record))
        self.assertEqual(self.store.db.execute('SELECT count(*) FROM samples').fetchone()[0],1)
    def test_conflict_does_not_overwrite(self):
        self.store.persist(s.parse_data(FRAME))
        with self.assertRaises(ValueError):self.store.persist((ID,1,3500,1,1))
        self.assertEqual(self.store.db.execute('SELECT battery_mv FROM samples').fetchone()[0],3600)
    def test_tcp_fragmentation_and_durable_ack(self):
        with s.Server(('127.0.0.1',0),self.store,quiet=True) as server:
            thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
            try:
                with socket.create_connection(server.server_address,timeout=3) as connection:
                    for part in [FRAME[:2],FRAME[2:19],FRAME[19:]]:connection.sendall(part)
                    self.assertEqual(connection.makefile('rb').readline(),f'ACK,{ID},1\n'.encode())
                    # A separate connection can see the row before the client gets ACK.
                    with sqlite3.connect(self.path) as reader:self.assertEqual(reader.execute('SELECT count(*) FROM samples').fetchone()[0],1)
            finally:server.shutdown();thread.join(3)

if __name__=='__main__':unittest.main()
