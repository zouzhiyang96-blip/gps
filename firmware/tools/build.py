"""Python-only build frontend. Requires GNU Arm Embedded GCC, no make/CMake."""
from pathlib import Path
import argparse,json,os,shutil,subprocess

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--cc',default=os.environ.get('ARM_CC','arm-none-eabi-gcc'))
p.add_argument('--hal-dir',type=Path,default=ROOT/'vendor'/'hal')
p.add_argument('--device-dir',type=Path,default=ROOT/'vendor'/'device')
p.add_argument('--core-dir',type=Path,default=ROOT/'vendor'/'core')
p.add_argument('--config',type=Path)
args=p.parse_args()
def relative(v):return os.path.relpath(Path(v).resolve(),ROOT)
def run(cmd):subprocess.run([str(x) for x in cmd],check=True)
cc=str(Path(shutil.which(args.cc) or args.cc).resolve())
hal=args.hal_dir.resolve();device=args.device_dir.resolve();core=args.core_dir.resolve()
config=args.config.resolve() if args.config else None
os.chdir(ROOT)
for v in [hal/'Inc'/'stm32l4xx_hal.h',device/'Include'/'stm32l476xx.h',core/'core_cm4.h']:
    if not v.is_file():raise SystemExit(f'Missing dependency {v}. Run tools/fetch_vendor.py first.')
out=ROOT/'build';out.mkdir(exist_ok=True)
common=['-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-Os','-g3',
        '-ffunction-sections','-fdata-sections','-DSTM32L476xx','-DUSE_HAL_DRIVER',
        '-Iinclude','-I'+relative(hal/'Inc'),'-I'+relative(device/'Include'),'-I'+relative(core)]
if config:common+=['-include',relative(config)]
own=sorted((ROOT/'src').glob('*.c'))
modules=['hal','hal_cortex','hal_gpio','hal_rcc','hal_rcc_ex','hal_pwr','hal_pwr_ex',
         'hal_flash','hal_flash_ex','hal_flash_ramfunc','hal_rtc','hal_rtc_ex',
         'hal_uart','hal_uart_ex','hal_adc','hal_adc_ex','hal_dma','hal_dma_ex']
vendor=[hal/'Src'/('stm32l4xx_'+name+'.c') for name in modules]
vendor+=[device/'Source'/'Templates'/'system_stm32l4xx.c']
objects=[]
for source in own+vendor:
    obj=out/(source.stem+'.o');objects.append(relative(obj))
    strict=['-Wall','-Wextra','-Werror'] if source in own else []
    run([cc,*common,'-std=c11',*strict,'-c',relative(source),'-o',relative(obj)])
startup=device/'Source'/'Templates'/'gcc'/'startup_stm32l476xx.s'
obj=out/'startup.o';objects.append(relative(obj))
run([cc,*common,'-x','assembler-with-cpp','-c',relative(startup),'-o',relative(obj)])
elf='build/gps.elf'
multi=subprocess.check_output([cc,*common[:4],'-print-multi-directory'],text=True).strip()
version=subprocess.check_output([cc,'-dumpfullversion'],text=True).strip()
tool_root=Path(cc).parent.parent
library_dirs=[tool_root/'arm-none-eabi'/'lib'/multi,tool_root/'lib'/'gcc'/'arm-none-eabi'/version/multi]
library_flags=['-L'+relative(d) for d in library_dirs if d.is_dir()]
run([cc,*common,*library_flags,'-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Tld/stm32l476rg.ld',
     '-Wl,--gc-sections,-Map=build/gps.map','-Wl,--print-memory-usage',*objects,'-lc','-lm','-o',elf])
bin_dir=Path(cc).parent
suffix='.exe' if os.name=='nt' else ''
objcopy=bin_dir/('arm-none-eabi-objcopy'+suffix)
size=bin_dir/('arm-none-eabi-size'+suffix)
run([objcopy,'-O','ihex',elf,'build/gps.hex'])
run([size,elf])
print('Built build/gps.elf and build/gps.hex; command table at 0x080C0000.')
