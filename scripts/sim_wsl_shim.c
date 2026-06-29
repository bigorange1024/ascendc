/*
 * WSL SIM shims for CANN camodel without NPU driver:
 *  - redirect /etc/ascend_install.info
 *  - stub aclrtGetSocName + ACL init/memory/stream (malloc path)
 *  - stub rtGetC2cCtrlAddr / rtCtxGetCurrent for kernel launch without real device ctx
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int aclError;
typedef void *aclrtStream;
typedef void *aclrtContext;
typedef int32_t rtError_t;
typedef void *rtContext_t;

enum { ACL_ERROR_NONE = 0 };
enum { ACL_MEM_MALLOC_HUGE_FIRST = 0 };
enum { ACL_MEMCPY_HOST_TO_DEVICE = 1, ACL_MEMCPY_DEVICE_TO_HOST = 2 };
enum { RT_ERROR_NONE = 0 };

static const char kSocName[] = "Ascend910B4";
static const char kInstallInfoRedirect[] =
    "/home/yuanye/ascendc/scripts/local_etc/ascend_install.info";

static char g_fake_ctx[256];
static uint64_t g_c2c_ctrl_buf[16];

static const char *install_info_redirect(void)
{
    const char *repo = getenv("ASCENDC_REPO_ROOT");
    static char path[512];
    if (repo != NULL && repo[0] != '\0') {
        snprintf(path, sizeof(path), "%s/scripts/local_etc/ascend_install.info", repo);
        return path;
    }
    return kInstallInfoRedirect;
}

static const char *redirect_install_info(const char *path)
{
    if (path != NULL && strcmp(path, "/etc/ascend_install.info") == 0) {
        return install_info_redirect();
    }
    return path;
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
    pathname = redirect_install_info(pathname);
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
    pathname = redirect_install_info(pathname);
    if (flags & O_CREAT) {
        return real_openat(dirfd, pathname, flags, mode);
    }
    return real_openat(dirfd, pathname, flags);
}

rtError_t rtGetC2cCtrlAddr(uint64_t *addr, uint32_t *len)
{
    if (addr) {
        *addr = (uint64_t)(uintptr_t)g_c2c_ctrl_buf;
    }
    if (len) {
        *len = (uint32_t)sizeof(g_c2c_ctrl_buf);
    }
    return RT_ERROR_NONE;
}

rtError_t rtCtxGetCurrent(rtContext_t *ctx)
{
    if (ctx) {
        *ctx = g_fake_ctx;
    }
    return RT_ERROR_NONE;
}

rtError_t rtCtxSetCurrent(rtContext_t ctx)
{
    if (ctx != NULL) {
        memcpy(g_fake_ctx, ctx, sizeof(g_fake_ctx));
    }
    return RT_ERROR_NONE;
}

aclError aclInit(const char *config)
{
    (void)config;
    return ACL_ERROR_NONE;
}

aclError aclrtSetDevice(int32_t deviceId)
{
    (void)deviceId;
    return ACL_ERROR_NONE;
}

aclError aclrtCreateContext(aclrtContext *context, int32_t deviceId)
{
    (void)deviceId;
    if (context) {
        *context = g_fake_ctx;
    }
    return ACL_ERROR_NONE;
}

aclError aclrtCreateStream(aclrtStream *stream)
{
    static char dummy_stream;
    if (stream) {
        *stream = &dummy_stream;
    }
    return ACL_ERROR_NONE;
}

aclError aclrtMallocHost(void **hostPtr, size_t size)
{
    if (!hostPtr) {
        return ACL_ERROR_NONE;
    }
    *hostPtr = malloc(size > 0 ? size : 1);
    return ACL_ERROR_NONE;
}

aclError aclrtMalloc(void **devPtr, size_t size, int policy)
{
    (void)policy;
    if (!devPtr) {
        return ACL_ERROR_NONE;
    }
    *devPtr = malloc(size > 0 ? size : 1);
    return ACL_ERROR_NONE;
}

aclError aclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, int kind)
{
    (void)kind;
    if (dst && src && count > 0 && destMax >= count) {
        memcpy(dst, src, count);
    }
    return ACL_ERROR_NONE;
}

aclError aclrtSynchronizeStream(aclrtStream stream)
{
    (void)stream;
    return ACL_ERROR_NONE;
}

aclError aclrtFree(void *devPtr)
{
    free(devPtr);
    return ACL_ERROR_NONE;
}

aclError aclrtFreeHost(void *hostPtr)
{
    free(hostPtr);
    return ACL_ERROR_NONE;
}

aclError aclrtDestroyStream(aclrtStream stream)
{
    (void)stream;
    return ACL_ERROR_NONE;
}

aclError aclrtDestroyContext(aclrtContext context)
{
    (void)context;
    return ACL_ERROR_NONE;
}

aclError aclrtResetDevice(int32_t deviceId)
{
    (void)deviceId;
    return ACL_ERROR_NONE;
}

aclError aclFinalize(void)
{
    return ACL_ERROR_NONE;
}

const char *aclrtGetSocName(void)
{
    return kSocName;
}

void ascend_dump_sim_stub_anchor(void)
{
}
