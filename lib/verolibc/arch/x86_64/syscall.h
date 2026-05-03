#ifndef VERO_SYSCALL_H
#define VERO_SYSCALL_H

#include <stddef.h>
#include <stdint.h>

/* Linux x86_64 syscall numbers */
#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_stat        4
#define SYS_fstat       5
#define SYS_lseek       8
#define SYS_mmap        9
#define SYS_munmap      11
#define SYS_brk         12
#define SYS_mprotect    10
#define SYS_exit        60
#define SYS_exit_group  231
#define SYS_getpid      39
#define SYS_getuid      102
#define SYS_gettid      186
#define SYS_clock_gettime 228
#define SYS_nanosleep   35
#define SYS_fork        57
#define SYS_execve      59
#define SYS_wait4       61
#define SYS_kill        62
#define SYS_getcwd      79
#define SYS_chdir       80
#define SYS_rename      82
#define SYS_mkdir       83
#define SYS_rmdir       84
#define SYS_unlink      87
#define SYS_readlink    89
#define SYS_chmod       90
#define SYS_dup         32
#define SYS_dup2        33
#define SYS_pipe        22
#define SYS_socket      41
#define SYS_connect     42
#define SYS_accept      43
#define SYS_sendto      44
#define SYS_recvfrom    45
#define SYS_bind        49
#define SYS_listen      50

static inline long __syscall0(long n) {
    long r;
    __asm__ volatile ("syscall"
        : "=a"(r) : "0"(n)
        : "memory", "rcx", "r11");
    return r;
}
static inline long __syscall1(long n, long a1) {
    long r;
    __asm__ volatile ("syscall"
        : "=a"(r) : "0"(n), "D"(a1)
        : "memory", "rcx", "r11");
    return r;
}
static inline long __syscall2(long n, long a1, long a2) {
    long r;
    __asm__ volatile ("syscall"
        : "=a"(r) : "0"(n), "D"(a1), "S"(a2)
        : "memory", "rcx", "r11");
    return r;
}
static inline long __syscall3(long n, long a1, long a2, long a3) {
    long r;
    register long _a3 __asm__("rdx") = a3;
    __asm__ volatile ("syscall"
        : "=a"(r) : "0"(n), "D"(a1), "S"(a2), "r"(_a3)
        : "memory", "rcx", "r11");
    return r;
}
static inline long __syscall4(long n, long a1, long a2,
                               long a3, long a4) {
    long r;
    register long _a3 __asm__("rdx") = a3;
    register long _a4 __asm__("r10") = a4;
    __asm__ volatile ("syscall"
        : "=a"(r) : "0"(n), "D"(a1), "S"(a2), "r"(_a3), "r"(_a4)
        : "memory", "rcx", "r11");
    return r;
}
static inline long __syscall5(long n, long a1, long a2,
                               long a3, long a4, long a5) {
    long r;
    register long _a3 __asm__("rdx") = a3;
    register long _a4 __asm__("r10") = a4;
    register long _a5 __asm__("r8")  = a5;
    __asm__ volatile ("syscall"
        : "=a"(r) : "0"(n), "D"(a1), "S"(a2),
          "r"(_a3), "r"(_a4), "r"(_a5)
        : "memory", "rcx", "r11");
    return r;
}
static inline long __syscall6(long n, long a1, long a2, long a3,
                               long a4, long a5, long a6) {
    long r;
    register long _a3 __asm__("rdx") = a3;
    register long _a4 __asm__("r10") = a4;
    register long _a5 __asm__("r8")  = a5;
    register long _a6 __asm__("r9")  = a6;
    __asm__ volatile ("syscall"
        : "=a"(r) : "0"(n), "D"(a1), "S"(a2),
          "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6)
        : "memory", "rcx", "r11");
    return r;
}

#define vero_syscall(n,...)      __syscall##__VA_OPT__(1)(n,##__VA_ARGS__)
#define vero_write(fd,buf,n)     __syscall3(SYS_write,(long)(fd),(long)(buf),(long)(n))
#define vero_read(fd,buf,n)      __syscall3(SYS_read,(long)(fd),(long)(buf),(long)(n))
#define __vero_exit_raw(code)          __syscall1(SYS_exit_group,(long)(code))
#define vero_brk(addr)           __syscall1(SYS_brk,(long)(addr))
#define vero_mmap(a,l,p,f,d,o)  __syscall6(SYS_mmap,(long)(a),(long)(l),(long)(p),(long)(f),(long)(d),(long)(o))
#define vero_munmap(a,l)         __syscall2(SYS_munmap,(long)(a),(long)(l))
#define __vero_getpid_raw()            __syscall0(SYS_getpid)
#define __vero_open_raw(p,f,m)         __syscall3(SYS_open,(long)(p),(long)(f),(long)(m))
#define vero_close(fd)           __syscall1(SYS_close,(long)(fd))

#endif /* VERO_SYSCALL_H */
