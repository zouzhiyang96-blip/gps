"""Controlled TCP test endpoint. ACK is emitted only after SQLite commit."""
from pathlib import Path
import argparse,datetime,re,socketserver,sqlite3,threading

DATA=re.compile(rb'DATA,([0-9A-F]{24}),([0-9]{1,10}),([0-9]{1,5}),([0-9]{1,2}),([0-9]{1,10})\n')
MAX_LINE=160
def parse_data(frame):
    if len(frame)>MAX_LINE:raise ValueError('frame too long')
    match=DATA.fullmatch(frame)
    if not match:raise ValueError('invalid DATA frame')
    device=match[1].decode('ascii');sequence,mv,wake,boots=map(int,match.groups()[1:])
    if not (1<=sequence<0xFFFFFFFF and 1<=mv<=20000 and 1<=wake<=7 and 1<=boots<=0xFFFFFFFF):
        raise ValueError('field outside allowed range')
    return device,sequence,mv,wake,boots

class Store:
    def __init__(self,path):
        path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
        self.lock=threading.Lock()
        self.db=sqlite3.connect(path,check_same_thread=False)
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.execute('PRAGMA synchronous=FULL')
        self.db.execute('CREATE TABLE IF NOT EXISTS samples(device TEXT, sequence INTEGER, battery_mv INTEGER, wake INTEGER, boots INTEGER, received_utc TEXT, PRIMARY KEY(device,sequence))')
        self.db.commit()
    def persist(self,record):
        device,sequence,mv,wake,boots=record
        with self.lock,self.db:
            previous=self.db.execute('SELECT battery_mv,wake,boots FROM samples WHERE device=? AND sequence=?',(device,sequence)).fetchone()
            if previous is not None:
                if previous!=(mv,wake,boots):raise ValueError('record ID reused with a different payload')
                return False
            stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
            self.db.execute('INSERT INTO samples VALUES(?,?,?,?,?,?)',(*record,stamp))
        return True
    def close(self):self.db.close()

class Handler(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(15)
        try:
            while True:
                frame=self.rfile.readline(MAX_LINE+1)
                if not frame:return
                record=parse_data(frame)
                inserted=self.server.store.persist(record)
                if not self.server.quiet:print(f'{record[0]} seq={record[1]} '+('saved' if inserted else 'duplicate'),flush=True)
                if not self.server.drop_ack:
                    ack=f'ACK,{record[0]},{record[1]}\n'.encode('ascii')
                    if self.server.send_ping:ack+=b'CMD,42,1,\n'
                    self.wfile.write(ack);self.wfile.flush()
        except (ValueError,TimeoutError,OSError) as error:
            if not self.server.quiet:print('session ended: '+str(error),flush=True)

class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address=True
    daemon_threads=True
    def __init__(self,address,store,drop_ack=False,send_ping=False,quiet=False):
        self.store=store;self.drop_ack=drop_ack;self.send_ping=send_ping;self.quiet=quiet
        super().__init__(address,Handler)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host',default='127.0.0.1');p.add_argument('--port',type=int,default=9000)
    p.add_argument('--db',type=Path,default=Path('test-data/uplink.sqlite3'))
    p.add_argument('--drop-ack',action='store_true');p.add_argument('--send-ping',action='store_true')
    a=p.parse_args();store=Store(a.db)
    try:
        with Server((a.host,a.port),store,a.drop_ack,a.send_ping) as server:
            print(f'Listening on {a.host}:{a.port}. Lab plaintext protocol only.',flush=True)
            server.serve_forever()
    except KeyboardInterrupt:pass
    finally:store.close()
if __name__=='__main__':main()
