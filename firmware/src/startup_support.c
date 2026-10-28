/* GNU Arm -nostartfiles: CMSIS reset startup calls __libc_init_array(). */
#include <errno.h>
#include <sys/types.h>
void _init(void) {}
/* No stdin/stdout/filesystem on this target. Use port_log() for debug UART. */
int _write(int fd,const char *buffer,int count){(void)fd;(void)buffer;(void)count;errno=ENOSYS;return -1;}
int _read(int fd,char *buffer,int count){(void)fd;(void)buffer;(void)count;errno=ENOSYS;return -1;}
int _close(int fd){(void)fd;errno=EBADF;return -1;}
off_t _lseek(int fd,off_t offset,int origin){(void)fd;(void)offset;(void)origin;errno=ESPIPE;return (off_t)-1;}
