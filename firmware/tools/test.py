from pathlib import Path
import argparse,os,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',default=os.environ.get('CC','cc'));args=p.parse_args()
cc=str(Path(shutil.which(args.cc) or args.cc).resolve());os.chdir(ROOT)
out=ROOT/'build';out.mkdir(exist_ok=True)
binary='build/test_core'+('.exe' if os.name=='nt' else '')
sources=['src/app.c','src/journal.c','src/protocol.c','src/commands.c','src/modem_ec200u.c','tests/test_core.c']
subprocess.run([cc,'-std=c99','-Wall','-DHOST_TEST','-Iinclude','-include','tests/test_config.h',*sources,'-o',binary],check=True)
subprocess.run([str(ROOT/binary)],check=True)
