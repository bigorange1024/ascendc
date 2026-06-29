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
| [**pass-toy-mix-s123-byteencode-k2/**](pass-toy-mix-s123-byteencode-k2/) | **MIX 玩具**：双 AIV S1 → Cube 64³ → UB Adds+func1（无跨 AIV） | ✓ | ✓ |

Phase A 早期 harness 已归档：[`frozen/frozen-f203-ntt-phase-a-fsm/`](frozen/frozen-f203-ntt-phase-a-fsm/)（2026-06-19，任务完成非路线否决）。

**MLKEM NTT + 向量集成**：向量全链路 **[vec-k4-v2](pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)**；**8-poly 批 NTT/INTT** **[pass-fix-f203-stage123-ntt-intt-polyvec8-vec](pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)**；标量对照组 [`frozen-fix-f203-2s1e-alg13-16171820-k4`](frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)（2026-06-19 归档，任务完成）。预研 [`exp-k4`](../examples/incubating/exp-mlkem-f203-alg13-16171820-2s1e-k4/)。见 [MLKEM-NTT-向量与标量实现指南.md](../docs/notes/MLKEM-NTT-向量与标量实现指南.md)。

---

## 规划中

| 目录 | 说明 |
|------|------|
| **vec-k4-v3**（暂定） | fork v2；接入设备 **`src` + `a_hat`**（上行已 PASS：[`lines8-15-se-k4`](pass-fix-f203-alg13-lines8-15-se-k4/) + [`lines3-7-a-hat-k4`](pass-fix-f203-alg13-lines3-7-a-hat-k4/)） |
| [**fix-f203-alg6-bytedecode-d-vec-k4/**](fix-f203-alg6-bytedecode-d-vec-k4/) | **Alg.6 ByteDecode_d** 向量（Decrypt c₁/c₂ 解包；d=4 SIM **9186**） |
| [**fix-f203-byteencode-d-vec-k4/**](fix-f203-byteencode-d-vec-k4/) | **Alg.5 ByteEncode_d** 向量（Encrypt c₁/c₂ 打包；d=4 SIM **5435**） |
| [**fix-f203-compress-d-vec-k4/**](fix-f203-compress-d-vec-k4/) | **§4.2.1 Compress_d** 向量（Alg.14 前半；d=4 SIM **3247**） |
| [**fix-f203-decompress-d-vec-k4/**](fix-f203-decompress-d-vec-k4/) | **§4.2.1 Decompress_d** 向量（Alg.15 前半；d=4 SIM **3177**） |
| [**fix-f203-alg14-pke-encrypt-correctness-k4/**](fix-f203-alg14-pke-encrypt-correctness-k4/) | **Alg.14 PKE Encrypt 设备拼装**（**G5 CPU 全链 ✅**；SIM G1–G3 ✅ / **SIM c.bin 阻塞**；见 [`STATUS.md`](fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md)） |

**ByteEncode₁₂（KeyGen）**：保持 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](pass-fix-f203-2s1e-byteencode12-vec-k4/) 不变；Encrypt 侧 **d=4/10** 见 [`fix-f203-byteencode-d-vec-k4`](fix-f203-byteencode-d-vec-k4/)。

---

## 已关闭路线（frozen）

Alg.13 **Phase A 全链 benchmark**（2026-06-28）、**host-scalar / se-device-scalar**（2026-06-26）、merged_kyber、Alg.13 sepair、Tag5T polybatch-s123、**basemul-vec spike**、**vec-k4 v1**、**innerproduct 二期 half** 等已迁入 [frozen/INDEX.md](frozen/INDEX.md)。**只读 `FROZEN.md` 知悉归档原因**；禁止抄 frozen 实现。活跃 MLKEM 向量见上表 **vec-k4-v2**。
