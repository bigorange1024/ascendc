/**
 * @file f203_kem_enc_prep_re_entry.cpp
 * @brief Alg.20 Encaps：融合 KEM 头（m/H/G）+ vendor Alg.14 prep_re（coins→r/e₁/e₂）。
 *
 * 背景：单 binary AIV-only func_key≤5；不新增独立 AIV 核，在 prep_re 入口先写 K/m/coins，
 * 再调用 vendor BuildReFromCoinsGm。注册符号 f203_kem_enc_prep_re（替换纯 Encrypt prep_re）。
 *
 * @param ek_gm / seed_d_gm / K_gm / m_gm / coins_gm 见 KemEncInitHead
 * @param prf_out_gm / re_gm / tiling vendor prep_re 几何
 */
#include "f203_encrypt_re_layout.h"
#include "f203_encrypt_re_vector.hpp"
#include "f203_kem_enc_init.hpp"

extern "C" __global__ __aicore__ void f203_kem_enc_prep_re(GM_ADDR ek_gm, GM_ADDR seed_d_gm, GM_ADDR K_gm, GM_ADDR m_gm,
                                                           GM_ADDR coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                           GM_ADDR tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    // 仅 block0：头段哈希 + 整批 PRF/CBD（与 Encrypt prep_re 单核约定一致）
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    // ① Alg.20 头：m / H(ek) / G → K、coins
    F203KemEnc::KemEncInitHead(reinterpret_cast<__gm__ uint8_t *>(ek_gm),
                               reinterpret_cast<__gm__ uint8_t *>(seed_d_gm),
                               reinterpret_cast<__gm__ uint8_t *>(K_gm),
                               reinterpret_cast<__gm__ uint8_t *>(m_gm),
                               reinterpret_cast<__gm__ uint8_t *>(coins_gm));

    // ② vendor：coins → r/e1/e2（Alg.14 Encrypt 噪声采样）
    const __gm__ uint8_t *coinsPtr = reinterpret_cast<const __gm__ uint8_t *>(coins_gm);
    F203EncryptRe::BuildReFromCoinsGm(coinsPtr, prf_out_gm, re_gm, tiling);
}

#ifndef __CCE_KT_TEST__
/** Host 侧 ACL launch 包装（与其它 *_do 同型）。 */
extern "C" void f203_kem_enc_prep_re_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm,
                                        uint8_t *seed_d_gm, uint8_t *K_gm, uint8_t *m_gm, uint8_t *coins_gm,
                                        uint8_t *prf_out_gm, uint8_t *re_gm, uint8_t *tiling)
{
    f203_kem_enc_prep_re<<<blockDim, l2ctrl, stream>>>(ek_gm, seed_d_gm, K_gm, m_gm, coins_gm, prf_out_gm, re_gm,
                                                     tiling);
}
#endif
