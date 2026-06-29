/* Redirect /etc/ascend_install.info for WSL SIM (no sudo).
 * CANN runtime opens the file via direct syscall, so we hook syscall(SYS_openat)
 * in addition to glibc open/openat/fopen. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static const char kDefaultPath[] =
    "/home/yuanye/ascendc/scripts/local_etc/ascend_install.info";

static long (*real_syscall)(long number, ...) = NULL;

static const char *install_info_path(void)
{
    const char *repo = getenv("ASCENDC_REPO_ROOT");
    static char path[512];
    if (repo != NULL && repo[0] != '\0') {
        snprintf(path, sizeof(path), "%s/scripts/local_etc/ascend_install.info", repo);
        return path;
    }
    return kDefaultPath;
}

static const char *redirect(const char *path)
{
    if (path != NULL && strcmp(path, "/etc/ascend_install.info") == 0) {
        return install_info_path();
    }
    return path;
}

static long forward_syscall(long number, va_list ap)
{
    long a1 = va_arg(ap, long);
    long a2 = va_arg(ap, long);
    long a3 = va_arg(ap, long);
    long a4 = va_arg(ap, long);
    long a5 = va_arg(ap, long);
    long a6 = va_arg(ap, long);
    return real_syscall(number, a1, a2, a3, a4, a5, a6);
}

long syscall(long number, ...)
{
    if (real_syscall == NULL) {
        real_syscall = (long (*)(long, ...))dlsym(RTLD_NEXT, "syscall");
    }

    va_list ap;
    va_start(ap, number);

    if (number == SYS_openat) {
        int dirfd = va_arg(ap, int);
        const char *pathname = va_arg(ap, const char *);
        int flags = va_arg(ap, int);
        pathname = redirect(pathname);
        if (flags & O_CREAT) {
            mode_t mode = va_arg(ap, mode_t);
            va_end(ap);
            return real_syscall(number, dirfd, pathname, flags, mode);
        }
        va_end(ap);
        return real_syscall(number, dirfd, pathname, flags);
    }

    long ret = forward_syscall(number, ap);
    va_end(ap);
    return ret;
}

int open(const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    int (*real_open)(const char *, int, ...) = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    pathname = redirect(pathname);
    if (flags & O_CREAT) {
        return real_open(pathname, flags, mode);
    }
    return real_open(pathname, flags);
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    int (*real_openat)(int, const char *, int, ...) =
        (int (*)(int, const char *, int, ...))dlsym(RTLD_NEXT, "openat");
    pathname = redirect(pathname);
    if (flags & O_CREAT) {
        return real_openat(dirfd, pathname, flags, mode);
    }
    return real_openat(dirfd, pathname, flags);
}

FILE *fopen(const char *pathname, const char *mode)
{
    FILE *(*real_fopen)(const char *, const char *) =
        (FILE * (*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen");
    return real_fopen(redirect(pathname), mode);
}

const char *aclrtGetSocName(void)
{
    return "Ascend910B4";
}

__attribute__((constructor)) static void sim_wsl_preload_ready(void)
{
    (void)install_info_path();
}
