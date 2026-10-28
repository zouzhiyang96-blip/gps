"""Fetch pinned vendor sources only. Does not install a compiler."""
from pathlib import Path
import argparse,concurrent.futures,hashlib,io,json,urllib.request,zipfile

ROOT=Path(__file__).resolve().parents[1]
HAL='047a8610c7dc0dcfdb4cd63b86cd3a2cbd3e5899'
DEVICE='dc04e41afbd7c204cd09f2fa5dd29658efbaff6c'
def get(url):
    request=urllib.request.Request(url,headers={'User-Agent':'gps-firmware/1.0'})
    with urllib.request.urlopen(request,timeout=60) as r:return r.read()
def archive(repo,ref,destination):
    if (destination/'Inc').exists() or (destination/'Include').exists():return
    data=get(f'https://codeload.github.com/{repo}/zip/{ref}')
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        prefix=z.namelist()[0].split('/')[0]+'/'
        for item in z.infolist():
            relative=item.filename.removeprefix(prefix)
            if not relative:continue
            target=(destination/relative).resolve();target.relative_to(destination.resolve())
            if item.is_dir():target.mkdir(parents=True,exist_ok=True)
            else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(z.read(item))
    print('Fetched '+repo+' @ '+ref)
def core():
    dst=ROOT/'vendor'/'core';dst.mkdir(parents=True,exist_ok=True)
    names=['core_cm4.h','cmsis_version.h','cmsis_compiler.h','cmsis_gcc.h','mpu_armv7.h',
           'cachel1_armv7.h','cmsis_armcc.h','cmsis_armclang.h','cmsis_armclang_ltm.h','cmsis_iccarm.h']
    for name in names:
        target=dst/name
        if not target.exists():target.write_bytes(get('https://raw.githubusercontent.com/ARM-software/CMSIS_5/5.9.0/CMSIS/Core/Include/'+name))
    # Keep upstream licenses next to downloaded headers.
    for name,url in [('CMSIS-LICENSE.txt','https://raw.githubusercontent.com/ARM-software/CMSIS_5/5.9.0/LICENSE.txt')]:
        target=dst/name
        if not target.exists():target.write_bytes(get(url))
    print('Fetched CMSIS Core 5.9.0')
if __name__=='__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        jobs=[pool.submit(archive,'STMicroelectronics/stm32l4xx-hal-driver',HAL,ROOT/'vendor'/'hal'),
              pool.submit(archive,'STMicroelectronics/cmsis-device-l4',DEVICE,ROOT/'vendor'/'device'),pool.submit(core)]
        for job in jobs:job.result()
