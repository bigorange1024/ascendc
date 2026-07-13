/**
 * @file main.cpp
 *
 * Copyright (C) 2023-2024. Huawei Technologies Co., Ltd. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#include "data_utils.h"
#include "tiling.h"
#include <stdexcept>
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling);
#endif

#ifdef ASCENDC_CPU_DEBUG
#include <stdio.h>
#define __print_array_short(x, len, line) \
{\
	printf("[%ld]\033[1;32mtensor[%s] \033[0mlen = %d, %s:%d\n", (long)-1, #x, len, __FILE__, __LINE__);\
	for(uint32_t i=0; i<(uint32_t)len; i++) {\
		printf("%4d ", (x)[i]);\
		if((i + 1) % line == 0) printf("\n");\
	}\
	printf("\n");\
}

#endif

int32_t main(int32_t argc, char *argv[])
{
    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::n * sizeof(int32_t);
    size_t dstFileSize = tiling::n * sizeof(int32_t);
    size_t matMFileSize = tiling::n * tiling::n * 4;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;
    
#ifdef ASCENDC_CPU_DEBUG
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if(tilingSize != sizeof(TilingData)) return 1;
    TilingData* tiling = (TilingData*) tiling_data;

    uint8_t *dst  = (uint8_t *)AscendC::GmAlloc(std::max(dstFileSize, (size_t)1024));
    uint8_t *src  = (uint8_t *)AscendC::GmAlloc(std::max(srcFileSize, (size_t)1024));
    uint8_t *ws   = (uint8_t *)AscendC::GmAlloc(std::max(wsFileSize, (size_t)1024));

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize); 
    if(!ok) return 9;
    ok = ReadFile("./input/M4.bin", matMFileSize, ws + tiling::M0, matMFileSize);
    if(!ok) return 10;
    ICPU_RUN_KF(mmad_custom, blockDim, dst, src, ws, *tiling);

    ok = WriteFile("./output/dst.bin", dst, dstFileSize);
    if(!ok) return 11;
    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)ws);
    AscendC::GmFree((void *)tiling_data);
#else
    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *srcHost, *wsHost;
    uint8_t *dstDevice, *srcDevice, *wsDevice;
    
    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), dstFileSize));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::cout << "[[[[[K]]]]]" << std::endl;
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    std::cout << "[[[[[K1]]]]]" << std::endl;
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    std::cout << "[[[[[K2]]]]]" << std::endl;
    ok = ReadFile("./input/src.bin", srcFileSize, srcHost, srcFileSize); 
    if(!ok) return 9;
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    std::cout << "[[[[[A]]]]]" << std::endl;
    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/M4.bin", matMFileSize, wsHost + tiling::M0, matMFileSize);
    if(!ok) return 10;
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    std::cout << "[[[[[B]]]]]" << std::endl;
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, dstDevice, srcDevice, wsDevice, tiling);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    std::cout << "[[[[[RUN]]]]]" << std::endl;

    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/dst.bin", dstHost, dstFileSize);
    if(!ok) return 11;

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
