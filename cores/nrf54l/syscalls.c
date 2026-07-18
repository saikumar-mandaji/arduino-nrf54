/**
 * @file syscalls.c
 * @brief Minimal newlib syscall stubs required to link a bare-metal image
 *        (no OS underneath). Standard boilerplate, not specific to this
 *        core's actual functionality -- only _sbrk is exercised (by
 *        malloc/new, if a sketch uses them); the rest exist purely to
 *        satisfy the linker.
 */
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

extern uint8_t __HeapBase;
extern uint8_t __HeapLimit;

static uint8_t * s_heap_end = &__HeapBase;

void * _sbrk(int incr)
{
    uint8_t * prev_heap_end = s_heap_end;

    if (s_heap_end + incr > &__HeapLimit)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    s_heap_end += incr;
    return (void *)prev_heap_end;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat * st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file; (void)ptr; (void)dir;
    return 0;
}

int _read(int file, char * ptr, int len)
{
    (void)file; (void)ptr; (void)len;
    return 0;
}

int _write(int file, const char * ptr, int len)
{
    (void)file; (void)ptr;
    return len;
}

void _exit(int status)
{
    (void)status;
    while (1) { }
}

int _kill(int pid, int sig)
{
    (void)pid; (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1;
}
