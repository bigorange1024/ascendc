/**
 * @file main.cpp
 * @brief Host 驱动：读 input bin → launch `mmad_custom` → 写 output bin。
 *
 * ## GM 参数顺序（与内核一致）
 *
 *   dst, t_hat, ek_out, sk_out, src, a_hat, ws, tiling
 *
 * ## workspace `ws` 布局（见 tiling.h）
 *
 *   LUT even/odd stacked → S0 [32,256] → mat_c 四路临时 → MAT_C_PLANAR [96,128]
 *
 * ## mixPass 预设输入（分阶段调试）
 *
 *   2 → s0_preset；3 → mat_c_preset；4/7 → dst_preset；7 → t_hat_preset
 *
 * CPU：`g_2s1e_mix_pass` 与 tiling.mixPass 同步；SIM/设备读 tiling。
 */
#include "data_utils.h"
#include "tiling.h"
#if F203_KEYGEN_EK_PKE >= 1
#include "f203_keygen_layout.h"
#endif
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#include "aclrtlaunch_mmad_custom.h"
#else
#include "tikicpulib.h"
#ifndef GM_ADDR
#define GM_ADDR int8_t *
#endif
extern "C" void mmad_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out, GM_ADDR src,
                            GM_ADDR a_hat, GM_ADDR ws, TilingData tiling
#if F203_KEYGEN_EK_PKE >= 1
                            ,
                            GM_ADDR rho_gm, GM_ADDR ek_pke_gm
#endif
);
extern volatile int g_2s1e_mix_pass;
#endif

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    size_t tilingSize = 64;
    static_assert(sizeof(TilingData) <= 64, "");
    size_t srcFileSize = tiling::srcFileBytes;
    size_t dstFileSize = tiling::dstFileBytes;
    size_t tHatFileSize = tiling::tHatFileBytes;
    size_t aHatFileSize = tiling::aHatFileBytes;
    size_t ekFileSize = byte_encode::polyVecBytes;
    size_t skFileSize = byte_encode::polyVecBytes;
#if F203_KEYGEN_EK_PKE >= 1
    const size_t rhoFileSize = F203Keygen::kRhoBytes;
    const size_t ekPkeFileSize = F203Keygen::kEkPkeBytes;
#endif
    size_t lutFileSize = tiling::lutEvenOddFileBytes;
    size_t matCFileSize = tiling::matCFileBytes;
    size_t s0FileSize = tiling::s0FileBytes;
    uint32_t blockDim = 1;
    const size_t wsFileSize = tiling::wssize;
    bool ok;

#ifdef ASCENDC_CPU_DEBUG
    /* ---------- CPU tikicpu：GmAlloc + ReadFile + ICPU_RUN_KF ---------- */
    AscendC::SetKernelMode(KernelMode::MIX_MODE);
    uint8_t *tiling_data = (uint8_t *)AscendC::GmAlloc(tilingSize);
    ReadFile("./input/tiling.bin", tilingSize, tiling_data, tilingSize);
    if (tilingSize < sizeof(TilingData)) {
        return 1;
    }
    TilingData *tiling = (TilingData *)tiling_data;
    g_2s1e_mix_pass = tiling->mixPass;

    uint8_t *dst = (uint8_t *)AscendC::GmAlloc(dstFileSize > 1024 ? dstFileSize : 1024);
    uint8_t *t_hat = (uint8_t *)AscendC::GmAlloc(tHatFileSize > 1024 ? tHatFileSize : 1024);
    uint8_t *ek_out = (uint8_t *)AscendC::GmAlloc(ekFileSize > 1024 ? ekFileSize : 1024);
    uint8_t *sk_out = (uint8_t *)AscendC::GmAlloc(skFileSize > 1024 ? skFileSize : 1024);
    uint8_t *src = (uint8_t *)AscendC::GmAlloc(srcFileSize > 1024 ? srcFileSize : 1024);
    uint8_t *a_hat = (uint8_t *)AscendC::GmAlloc(aHatFileSize > 1024 ? aHatFileSize : 1024);
    uint8_t *ws = (uint8_t *)AscendC::GmAlloc(wsFileSize > 1024 ? wsFileSize : 1024);
    for (size_t i = 0; i < wsFileSize; ++i) {
        ws[i] = 0;
    }

    ok = ReadFile("./input/src.bin", srcFileSize, src, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/a_hat.bin", aHatFileSize, a_hat, aHatFileSize);
    if (!ok) {
        return 16;
    }
    ok = ReadFile("./input/lut_even_stacked.bin", lutFileSize, ws + tiling::LUT_EVEN_STACKED, lutFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/lut_odd_stacked.bin", lutFileSize, ws + tiling::LUT_ODD_STACKED, lutFileSize);
    if (!ok) {
        return 10;
    }
    if (tiling->mixPass == 2) {
        ok = ReadFile("./input/s0_preset.bin", s0FileSize, ws + tiling::S0, s0FileSize);
        if (!ok) {
            return 13;
        }
    }
    if (tiling->mixPass == 6) {
        size_t sanityS0Read = ::tiling::sanityS0Bytes;
        ok = ReadFile("./input/s0_sanity.bin", sanityS0Read, ws + ::tiling::S0, ::tiling::sanityS0Bytes);
        if (!ok) {
            return 13;
        }
    }
    if (tiling->mixPass == 3) {
        ok = ReadFile("./input/mat_c_preset.bin", matCFileSize, ws + tiling::MAT_C, matCFileSize);
        if (!ok) {
            return 14;
        }
    }
    if (tiling->mixPass == 4 || tiling->mixPass == 7) {
        ok = ReadFile("./input/dst_preset.bin", dstFileSize, dst, dstFileSize);
        if (!ok) {
            return 17;
        }
    }
    if (tiling->mixPass == 7) {
        ok = ReadFile("./input/t_hat_preset.bin", tHatFileSize, t_hat, tHatFileSize);
        if (!ok) {
            return 18;
        }
    }

#if F203_KEYGEN_EK_PKE >= 1
    uint8_t *rho = (uint8_t *)AscendC::GmAlloc(rhoFileSize > 32 ? rhoFileSize : 32);
    uint8_t *ek_pke = (uint8_t *)AscendC::GmAlloc(ekPkeFileSize > 1024 ? ekPkeFileSize : 1024);
    size_t rhoRead = rhoFileSize;
    ok = ReadFile("./input/rho.bin", rhoRead, rho, rhoFileSize);
    if (!ok) {
        return 22;
    }
    ICPU_RUN_KF(mmad_custom, blockDim, dst, t_hat, ek_out, sk_out, src, a_hat, ws, *tiling, rho, ek_pke);
#else
    ICPU_RUN_KF(mmad_custom, blockDim, dst, t_hat, ek_out, sk_out, src, a_hat, ws, *tiling);
#endif

    ok = WriteFile("./output/dst.bin", dst, dstFileSize);
    if (!ok) {
        return 11;
    }
    ok = WriteFile("./output/t_hat.bin", t_hat, tHatFileSize);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/ek_polyvec.bin", ek_out, ekFileSize);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/sk_polyvec.bin", sk_out, skFileSize);
    if (!ok) {
        return 21;
    }
#if F203_KEYGEN_EK_PKE >= 1
    ok = WriteFile("./output/ek_pke.bin", ek_pke, ekPkeFileSize);
    if (!ok) {
        return 23;
    }
#endif
    ok = WriteFile("./output/s0.bin", ws + tiling::S0, s0FileSize);
    if (!ok) {
        return 12;
    }
    if (tiling->mixPass == 6) {
        ok = WriteFile("./output/mat_c_tmp_lo_even.bin", ws + ::tiling::MAT_C_TMP_LO_EVEN, ::tiling::sanityMatCTmpBytes);
        if (!ok) {
            return 15;
        }
    }
    ok = WriteFile("./output/mat_c.bin", ws + tiling::MAT_C, matCFileSize);
    if (!ok) {
        return 15;
    }
    AscendC::GmFree((void *)dst);
    AscendC::GmFree((void *)t_hat);
    AscendC::GmFree((void *)ek_out);
    AscendC::GmFree((void *)sk_out);
    AscendC::GmFree((void *)src);
    AscendC::GmFree((void *)a_hat);
    AscendC::GmFree((void *)ws);
#if F203_KEYGEN_EK_PKE >= 1
    AscendC::GmFree((void *)rho);
    AscendC::GmFree((void *)ek_pke);
#endif
    AscendC::GmFree((void *)tiling_data);
#else
    /* ---------- SIM/NPU：aclrtMalloc + ACLRT_LAUNCH_KERNEL + D2H ---------- */
    CHECK_ACL(aclInit(nullptr));
    aclrtContext context;
    int32_t deviceId = 0;
    CHECK_ACL(aclrtSetDevice(deviceId));
    CHECK_ACL(aclrtCreateContext(&context, deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    uint8_t *dstHost, *tHatHost, *ekHost, *skHost, *srcHost, *aHatHost, *wsHost;
    uint8_t *dstDevice, *tHatDevice, *ekDevice, *skDevice, *srcDevice, *aHatDevice, *wsDevice;

    TilingData *tiling;
    CHECK_ACL(aclrtMallocHost((void **)(&tiling), tilingSize));
    ReadFile("./input/tiling.bin", tilingSize, tiling, tilingSize);

    CHECK_ACL(aclrtMallocHost((void **)(&dstHost), dstFileSize));
    CHECK_ACL(aclrtMalloc((void **)&dstDevice, dstFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&tHatHost), tHatFileSize));
    CHECK_ACL(aclrtMalloc((void **)&tHatDevice, tHatFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&ekHost), ekFileSize));
    CHECK_ACL(aclrtMalloc((void **)&ekDevice, ekFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&skHost), skFileSize));
    CHECK_ACL(aclrtMalloc((void **)&skDevice, skFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&srcHost), srcFileSize));
    CHECK_ACL(aclrtMalloc((void **)&srcDevice, srcFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost((void **)(&aHatHost), aHatFileSize));
    CHECK_ACL(aclrtMalloc((void **)&aHatDevice, aHatFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

    ok = ReadFile("./input/src.bin", srcFileSize, srcHost, srcFileSize);
    if (!ok) {
        return 9;
    }
    ok = ReadFile("./input/a_hat.bin", aHatFileSize, aHatHost, aHatFileSize);
    if (!ok) {
        return 16;
    }
    CHECK_ACL(aclrtMemcpy(srcDevice, srcFileSize, srcHost, srcFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    CHECK_ACL(aclrtMemcpy(aHatDevice, aHatFileSize, aHatHost, aHatFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

    CHECK_ACL(aclrtMallocHost((void **)(&wsHost), wsFileSize));
    CHECK_ACL(aclrtMalloc((void **)&wsDevice, wsFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    ok = ReadFile("./input/lut_even_stacked.bin", lutFileSize, wsHost + tiling::LUT_EVEN_STACKED, lutFileSize);
    if (!ok) {
        return 10;
    }
    ok = ReadFile("./input/lut_odd_stacked.bin", lutFileSize, wsHost + tiling::LUT_ODD_STACKED, lutFileSize);
    if (!ok) {
        return 10;
    }
    if (tiling->mixPass == 2) {
        ok = ReadFile("./input/s0_preset.bin", s0FileSize, wsHost + tiling::S0, s0FileSize);
        if (!ok) {
            return 13;
        }
    }
    if (tiling->mixPass == 6) {
        size_t sanityS0Read = ::tiling::sanityS0Bytes;
        ok = ReadFile("./input/s0_sanity.bin", sanityS0Read, wsHost + ::tiling::S0, ::tiling::sanityS0Bytes);
        if (!ok) {
            return 13;
        }
    }
    if (tiling->mixPass == 3) {
        ok = ReadFile("./input/mat_c_preset.bin", matCFileSize, wsHost + tiling::MAT_C, matCFileSize);
        if (!ok) {
            return 14;
        }
    }
    if (tiling->mixPass == 4 || tiling->mixPass == 7) {
        ok = ReadFile("./input/dst_preset.bin", dstFileSize, dstHost, dstFileSize);
        if (!ok) {
            return 17;
        }
        CHECK_ACL(aclrtMemcpy(dstDevice, dstFileSize, dstHost, dstFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    }
    if (tiling->mixPass == 7) {
        ok = ReadFile("./input/t_hat_preset.bin", tHatFileSize, tHatHost, tHatFileSize);
        if (!ok) {
            return 18;
        }
        CHECK_ACL(aclrtMemcpy(tHatDevice, tHatFileSize, tHatHost, tHatFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    }
    CHECK_ACL(aclrtMemcpy(wsDevice, wsFileSize, wsHost, wsFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

#if F203_KEYGEN_EK_PKE >= 1
    uint8_t *rhoHost = nullptr;
    uint8_t *rhoDevice = nullptr;
    uint8_t *ekPkeHost = nullptr;
    uint8_t *ekPkeDevice = nullptr;
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&rhoHost), rhoFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&rhoDevice), rhoFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK_ACL(aclrtMallocHost(reinterpret_cast<void **>(&ekPkeHost), ekPkeFileSize));
    CHECK_ACL(aclrtMalloc(reinterpret_cast<void **>(&ekPkeDevice), ekPkeFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
    size_t rhoReadSim = rhoFileSize;
    ok = ReadFile("./input/rho.bin", rhoReadSim, rhoHost, rhoFileSize);
    if (!ok) {
        return 22;
    }
    CHECK_ACL(aclrtMemcpy(rhoDevice, rhoFileSize, rhoHost, rhoFileSize, ACL_MEMCPY_HOST_TO_DEVICE));
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, dstDevice, tHatDevice, ekDevice, skDevice, srcDevice, aHatDevice,
                                     wsDevice, tiling, rhoDevice, ekPkeDevice);
#else
    ACLRT_LAUNCH_KERNEL(mmad_custom)(blockDim, stream, dstDevice, tHatDevice, ekDevice, skDevice, srcDevice, aHatDevice,
                                     wsDevice, tiling);
#endif
    CHECK_ACL(aclrtSynchronizeStream(stream));

    CHECK_ACL(aclrtMemcpy(dstHost, dstFileSize, dstDevice, dstFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(tHatHost, tHatFileSize, tHatDevice, tHatFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(ekHost, ekFileSize, ekDevice, ekFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(skHost, skFileSize, skDevice, skFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_ACL(aclrtMemcpy(wsHost, wsFileSize, wsDevice, wsFileSize, ACL_MEMCPY_DEVICE_TO_HOST));

    ok = WriteFile("./output/dst.bin", dstHost, dstFileSize);
    if (!ok) {
        return 11;
    }
    ok = WriteFile("./output/t_hat.bin", tHatHost, tHatFileSize);
    if (!ok) {
        return 19;
    }
    ok = WriteFile("./output/ek_polyvec.bin", ekHost, ekFileSize);
    if (!ok) {
        return 20;
    }
    ok = WriteFile("./output/sk_polyvec.bin", skHost, skFileSize);
    if (!ok) {
        return 21;
    }
#if F203_KEYGEN_EK_PKE >= 1
    CHECK_ACL(aclrtMemcpy(ekPkeHost, ekPkeFileSize, ekPkeDevice, ekPkeFileSize, ACL_MEMCPY_DEVICE_TO_HOST));
    ok = WriteFile("./output/ek_pke.bin", ekPkeHost, ekPkeFileSize);
    if (!ok) {
        return 23;
    }
#endif
    ok = WriteFile("./output/s0.bin", wsHost + tiling::S0, s0FileSize);
    if (!ok) {
        return 12;
    }
    if (tiling->mixPass == 6) {
        ok = WriteFile("./output/mat_c_tmp_lo_even.bin", wsHost + ::tiling::MAT_C_TMP_LO_EVEN, ::tiling::sanityMatCTmpBytes);
        if (!ok) {
            return 15;
        }
    }
    ok = WriteFile("./output/mat_c.bin", wsHost + tiling::MAT_C, matCFileSize);
    if (!ok) {
        return 15;
    }

    CHECK_ACL(aclrtFree(dstDevice));
    CHECK_ACL(aclrtFreeHost(dstHost));
    CHECK_ACL(aclrtFree(tHatDevice));
    CHECK_ACL(aclrtFreeHost(tHatHost));
    CHECK_ACL(aclrtFree(ekDevice));
    CHECK_ACL(aclrtFreeHost(ekHost));
    CHECK_ACL(aclrtFree(skDevice));
    CHECK_ACL(aclrtFreeHost(skHost));
    CHECK_ACL(aclrtFree(srcDevice));
    CHECK_ACL(aclrtFreeHost(srcHost));
    CHECK_ACL(aclrtFree(aHatDevice));
    CHECK_ACL(aclrtFreeHost(aHatHost));
    CHECK_ACL(aclrtFree(wsDevice));
    CHECK_ACL(aclrtFreeHost(wsHost));
#if F203_KEYGEN_EK_PKE >= 1
    CHECK_ACL(aclrtFree(rhoDevice));
    CHECK_ACL(aclrtFreeHost(rhoHost));
    CHECK_ACL(aclrtFree(ekPkeDevice));
    CHECK_ACL(aclrtFreeHost(ekPkeHost));
#endif
    CHECK_ACL(aclrtFreeHost(tiling));

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtDestroyContext(context));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
#endif
    return 0;
}
