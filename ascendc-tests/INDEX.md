# ascendc-tests — AscendC 平台功能探针

| 规则 | 说明 |
|------|------|
| **目录名** | `<简述>/`，不加 `exp-` 前缀 |
| **`pass-` 前缀** | CPU + SIM 均已验收的**终态**探针加 `pass-`；进行中优化等保持 `fix-` |
| **KeyGen 子轨命名（2026-06-28）** | [`pass-fix-f203-alg7-sample-ntt-k4`](pass-fix-f203-alg7-sample-ntt-k4/)（Alg.7 单 poly）· [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](pass-fix-f203-alg13-lines3-7-a-hat-k4/)（行 3–7 `Â`）· [`pass-fix-f203-alg13-lines8-15-se-k4`](pass-fix-f203-alg13-lines8-15-se-k4/)（行 8–15 `s`/`e`→`src`）· [`pass-fix-f203-alg8-cbd-eta2-k4`](pass-fix-f203-alg8-cbd-eta2-k4/)（Alg.8 CBD η=2） |
| **状态** | 各目录 `STATUS.md`：`CPU` / `SIM` 列 |
| **文档** | 策略与纪要 → `docs/`、`qa/` |
| **废弃** | `frozen/frozen-*/` — **路线关闭**；见 [frozen/INDEX.md](frozen/INDEX.md)；禁止抄 frozen 源码与路线 |
| **SE 采样 golden** | [`library/shared/fips203_se_sample/golden_se_sampling.py`](../library/shared/fips203_se_sample/golden_se_sampling.py)（原 host-scalar / se-device-scalar 已冻结） |

**共用设备原语**（`library/shared/`）：SHAKE/Keccak AscendC、`fips203_se_sample`；见 [`../library/shared/INDEX.md`](../library/shared/INDEX.md)。

**自包含与设备全链**（2026-06-29）：活跃探针/example **不得**跨目录引用源码（`library/shared` 与仓库 `scripts/` CANN 壳除外）；KeyGen 生产路径禁止 Host 辅助密码计算。见 [用例自包含与设备全链约束.md](../docs/engineering/用例自包含与设备全链约束.md)；KeyGen 实例 [`pass-fix-f203-alg13-device-keygen-k4/SELF_CONTAINED.md`](pass-fix-f203-alg13-device-keygen-k4/SELF_CONTAINED.md)。

---

## 当前探针

| 目录 | 简述 | CPU | SIM |
|------|------|-----|-----|
| [add_custom/](add_custom/) | 向量加法冒烟 | — | — |
| [**pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/**](pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | **MLKEM 向量集成（活跃基线）**：NTT + 行 18 UB + ByteEncode prefetch；SIM **77958** tick | ✓ | ✓ |
| [**pass-fix-f203-alg11-12-multiplyntts-k4/**](pass-fix-f203-alg11-12-multiplyntts-k4/) | **Alg.11/12 向量核**：B2 + **`MEM_OPS=1` ROM DataCopy**（行 18 basemul 基线，SIM ~9k tick） | ✓ | ✓ |
| [**pass-fix-f203-alg11-12-innerproduct-k4/**](pass-fix-f203-alg11-12-innerproduct-k4/) | **4×4×1 全量内积**（单 AIV；SIM **43992**） | ✓ | ✓ |
| [**pass-fix-f203-alg11-12-innerproduct-k4-halfrows/**](pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) | **4×4×1 半行内积**（双 AIV，v2 行18 分片；SIM **26185**） | ✓ | ✓ |
| [**pass-fix-f203-2s1e-byteencode12-vec-k4/**](pass-fix-f203-2s1e-byteencode12-vec-k4/) | **ByteEncode₁₂ encode-only**；prefetch SIM **17429** / tile32 **25464** | ✓ | ✓ |
| [**pass-fix-f203-alg7-sample-ntt-k4/**](pass-fix-f203-alg7-sample-ntt-k4/) | **Alg.7 SampleNTT**（单 poly `(j,i)`→`â[256]`；672B→d12→rej）；SIM **~80100** tick | ✓ | ✓ |
| [**pass-fix-f203-alg13-lines3-7-a-hat-k4/**](pass-fix-f203-alg13-lines3-7-a-hat-k4/) | **Alg.13 行 3–7**：16×`Â`→`a_hat[16,256]`；默认双 AIV SIM **381544** tick | ✓ | ✓ |
| [**pass-fix-f203-alg13-lines8-15-se-k4/**](pass-fix-f203-alg13-lines8-15-se-k4/) | **Alg.13 行 8–15**：`SEED_D`→`src[8,256]`（G+P+C V3）；[`STATUS.md`](pass-fix-f203-alg13-lines8-15-se-k4/STATUS.md) | ✓ | ✓ |
| [**pass-fix-f203-alg13-device-keygen-k4/**](pass-fix-f203-alg13-device-keygen-k4/) | **Alg.13 全链 KeyGen**（prep 双 AIV 并行 Â；2 launch；SIM **542339** tick） | ✓ | ✓ |
| [**pass-fix-f203-alg8-cbd-eta2-k4/**](pass-fix-f203-alg8-cbd-eta2-k4/) | **Alg.8 CBD η=2**：默认 P2 双 AIV SIM **18048** tick（`bash run.sh -r sim`） | ✓ | ✓ |
| [**pass-fix-f203-stage123-ntt-intt-polyvec8-vec/**](pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | **8-poly 三段式 NTT/INTT**（紧凑 `[HI₈,LO₈]`；LUT 切换；1 launch；NTT SIM **30347** / INTT **30340**） | ✓ | ✓ |
| [**pass-shake128-ops-math-toy/**](pass-shake128-ops-math-toy/) | 共享核 **SHAKE128**（`*_toy_ub.hpp` 全 UB 参考） | ✓ | **12285** |
| [**pass-shake256-ascendc-toy/**](pass-shake256-ascendc-toy/) | 共享 `shake_xof_kernel` + **SHAKE256**（全 UB toy） | ✓ | **12285** |
| [**pass-f203-alg6-bytedecode-d-vec-k4/**](pass-f203-alg6-bytedecode-d-vec-k4/) | **Alg.6 ByteDecode_d**（**d=4/5/10/11** PASS；d=4 SIM **9186**，d=5 **5696**，d=11 **6641**） | ✓ | ✓ |
| [**pass-f203-byteencode-d-vec-k4/**](pass-f203-byteencode-d-vec-k4/) | **Alg.5 ByteEncode_d**（**d=4/5/10/11** PASS；d=4 SIM **5435**，d=5 **5537**，d=11 **6568**） | ✓ | ✓ |
| [**pass-f203-compress-d-vec-k4/**](pass-f203-compress-d-vec-k4/) | **§4.2.1 Compress_d**（**d=4/5/10/11** 向量；指南见 `docs/notes/F203-Compress-Decompress-向量实现指南.md`） | ✓ | ✓ |
| [**pass-f203-decompress-d-vec-k4/**](pass-f203-decompress-d-vec-k4/) | **§4.2.1 Decompress_d**（**d=4/5/10/11** 向量） | ✓ | ✓ |
| [**pass-toy-mix-s123-byteencode-k2/**](pass-toy-mix-s123-byteencode-k2/) | **MIX 玩具**：双 AIV S1 → Cube 64³ → UB Adds+func1（无跨 AIV） | ✓ | ✓ |
| [**pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/**](pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) | **Alg.14 prep**：`ek` 尾 ρ→`a_hat` + `coins`→`y/e₁/e₂`（单 launch，stable vendored）；**不含** `t̂` decode | ✓ | ✓ **470502** |
| [**pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/**](pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/) | **Alg.14 compute**：行 2/18/19/21（不含 μ）；SIM 单 launch **完成**；CPU 三 launch **部分**（û/u） | 部分 | ✓ **~125k** |
| [**fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/**](fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/) | **Alg.14 tail**：行 20 μ_embed + 行 22–24 pack→`c`（分组 ByteEncode；SIM **56259** tick） | ✓ | ✓ |

Phase A 早期 harness 已归档：[`frozen/frozen-f203-ntt-phase-a-fsm/`](frozen/frozen-f203-ntt-phase-a-fsm/)（2026-06-19，任务完成非路线否决）。

**MLKEM NTT + 向量集成**：向量全链路 **[vec-k4-v2](pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)**；**8-poly 批 NTT/INTT** **[pass-fix-f203-stage123-ntt-intt-polyvec8-vec](pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)**；标量对照组 [`frozen-fix-f203-2s1e-alg13-16171820-k4`](frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)（2026-06-19 归档，任务完成）。预研 [`exp-k4`](../examples/incubating/exp-mlkem-f203-alg13-16171820-2s1e-k4/)。见 [MLKEM-NTT-向量与标量实现指南.md](../docs/notes/MLKEM-NTT-向量与标量实现指南.md)。

---

## 规划中

| 目录 | 说明 |
|------|------|
| **vec-k4-v3**（暂定） | fork v2；接入设备 **`src` + `a_hat`**（上行已 PASS：[`lines8-15-se-k4`](pass-fix-f203-alg13-lines8-15-se-k4/) + [`lines3-7-a-hat-k4`](pass-fix-f203-alg13-lines3-7-a-hat-k4/)） |
| [**fix-f203-alg14-pke-encrypt-correctness-k4/**](fix-f203-alg14-pke-encrypt-correctness-k4/) | **Alg.14 Encrypt G5 ✅**（唯一活跃全链；G0–G4 → `frozen-gates/`）；CPU+SIM c.bin max=0；SIM tick **922441**；round-trip 见 [`scripts/roundtrip_pke_encrypt_decrypt.sh`](../scripts/roundtrip_pke_encrypt_decrypt.sh) |
| [**fix-f203-alg15-pke-decrypt-correctness-k4/**](fix-f203-alg15-pke-decrypt-correctness-k4/) | **Alg.15 Decrypt G4 ✅**（dk+c→m 全 device；**2 launch** prep \| ntt+intt）；CPU+SIM m.bin max=0；SIM tick **~427k**；同上 round-trip |
| [**fix-f203-alg19-kem-keygen-k4/**](fix-f203-alg19-kem-keygen-k4/) | **Alg.19 KEM KeyGen ✅**（d/z UB + vendor PKE + KeyGen_internal 尾段）；CPU+SIM+liboqs max=0；SIM **742558** tick |
| [**fix-f203-alg20-kem-encaps-k4/**](fix-f203-alg20-kem-encaps-k4/) | **Alg.20 KEM Encaps**（`ek`←alg19 · vendor Encrypt G5 · KEM 头并入 prep_re）；**CPU+SIM PASS** | ✓ | ✓ |
| [**fix-f203-alg21-kem-decaps-k4/**](fix-f203-alg21-kem-decaps-k4/) | **Alg.21 KEM Decaps**（dk+c→K；vendor D+E + **设备 FO**）；单设备库 · CPU 单 session `K max=0` PASS · **SIM 默认 2-session `K max=0` PASS** · 拒绝路径 `K=J(z‖c)` CPU PASS | ✓ | ✓（2-session） |

**KEM 端到端测试（仓库级 `scripts/`，镜像 PKE）**：`liboqs_kem_vs_ascendc.sh`（KeyGen→Encaps→Decaps→reject 四阶段逐级对 liboqs fixture）；**纯 device round-trip（分项，各跑一次，CPU/SIM 分开）**：`roundtrip_kem_keygen.sh` → `roundtrip_kem_encaps.sh` → `roundtrip_kem_decaps.sh`（stash `output/roundtrip_kem/<cpu|sim>/`）；一体入口 `roundtrip_kem_keygen_encaps_decaps.sh`（含拒绝路径）。SIM Decaps 2-session ~11min/段。

**KEM 分项 kat（固定 stash 密钥 + 每轮随机量，逐字节对 liboqs）**：`liboqs_kem_keygen_batch.sh`（`KEM_KG_EXT_SEED` 同 64B 种子）· `liboqs_kem_encaps_batch.sh`（`KEM_ENC_EXT_SEED` 旁路 `m`）· `liboqs_kem_decaps_batch.sh`（liboqs `encaps_derand` 造 `c`）；三者默认 `CPU×10+SIM×1`，均 **PASS**。密钥经 `kem_keypair_stash_bootstrap.sh` 落 `output/kem_keypair_stash/`。旁路宏均 test-only，生产默认关闭。
| ~~fix-f203-alg14-encrypt-2launch-k4~~ | **已冻结** → [`frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/`](frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/)（家里 agent `27cc93b`，办公室未复验） |

**ByteEncode₁₂（KeyGen）**：[`pass-fix-f203-2s1e-byteencode12-vec-k4`](pass-fix-f203-2s1e-byteencode12-vec-k4/)（**d=12**）。Encrypt/Decrypt 单算子 **d=4/10** 见上表 **`pass-f203-*-d4-d10-vec-k4`**；ml_kem_1024 的 **d=5/d=11** 在全链 Encrypt/Decrypt 探针内联。

---

## 已关闭路线（frozen）

Alg.13 **Phase A 全链 benchmark**（2026-06-28）、**host-scalar / se-device-scalar**（2026-06-26）、merged_kyber、Alg.13 sepair、Tag5T polybatch-s123、**basemul-vec spike**、**vec-k4 v1**、**innerproduct 二期 half** 等已迁入 [frozen/INDEX.md](frozen/INDEX.md)。**只读 `FROZEN.md` 知悉归档原因**；禁止抄 frozen 实现。活跃 MLKEM 向量见上表 **vec-k4-v2**。
