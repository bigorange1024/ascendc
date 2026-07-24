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

**多环境 `run.sh`**（2026-07-14）：活跃探针已 `source` [`scripts/runtime_env.sh`](../scripts/runtime_env.sh)（`-r auto|verify`、WSL 禁 npu）；见 [NPU真机环境说明.md](../docs/engineering/NPU真机环境说明.md) · [AscendC多环境运行纪要.md](../docs/notes/AscendC多环境运行纪要.md)。`frozen/` **不接入**；Decaps device（Phase-E）已接入。

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
| [**pass-f203-alg6-bytedecode-d-vec-k4/**](pass-f203-alg6-bytedecode-d-vec-k4/) | **Alg.6 ByteDecode_d**（**d=4/5/10/11** PASS；d=5/11 **标量 unpack**；宏见 [`F203-ByteEncode-ByteDecode-d-向量与标量选型`](../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md)） | ✓ | ✓ |
| [**pass-f203-byteencode-d-vec-k4/**](pass-f203-byteencode-d-vec-k4/) | **Alg.5 ByteEncode_d**（**d=4/5/10/11** PASS；d=5/11 **标量 pack**；`VEC=2` 保留不激活；同上笔记） | ✓ | ✓ |
| [**pass-f203-compress-d-vec-k4/**](pass-f203-compress-d-vec-k4/) | **§4.2.1 Compress_d**（**d=4/5/10/11**；**默认向量** `COMPRESS_D_VEC=1`；[`Compress-Decompress 指南`](../docs/notes/F203-Compress-Decompress-向量实现指南.md)） | ✓ | ✓ |
| [**pass-f203-compress-unified-int-vec-k4/**](pass-f203-compress-unified-int-vec-k4/) | **已迁至** [`exp-fips203-compress-unified-int-vec-k4`](../examples/incubating/exp-fips203-compress-unified-int-vec-k4/)（canonical + customspec） | ✓ | 部分 |
| [**pass-f203-decompress-d-vec-k4/**](pass-f203-decompress-d-vec-k4/) | **§4.2.1 Decompress_d**（**d=4/5/10/11**；**默认向量** `DECOMPRESS_D_VEC=1`；同上指南 + ByteEncode 选型笔记 §4） | ✓ | ✓ |
| [**pass-f203-decompress-unified-int-vec-k4/**](pass-f203-decompress-unified-int-vec-k4/) | **已迁至** [`exp-fips203-decompress-unified-int-vec-k4`](../examples/incubating/exp-fips203-decompress-unified-int-vec-k4/)（canonical + customspec） | ✓ | 部分 |
| [**pass-toy-mix-s123-byteencode-k2/**](pass-toy-mix-s123-byteencode-k2/) | **MIX 玩具**：双 AIV S1 → Cube 64³ → UB Adds+func1（无跨 AIV） | ✓ | ✓ |
| [**pass-merged-kyber-mix-ntt256/**](pass-merged-kyber-mix-ntt256/) | **授权示例**：Kyber 单 poly n=256 MIX NTT（AivSplit→AicMmad×2→Merge）；原 `thirdparty/merged_kyber`；**非** FIPS Tag5T；SIM **10348** tick | ✓ | ✓ |
| [**pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/**](pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) | **Alg.14 prep**：`ek` 尾 ρ→`a_hat` + `coins`→`y/e₁/e₂`（单 launch，stable vendored）；**不含** `t̂` decode | ✓ | ✓ **470502** |
| [**pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/**](pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/) | **Alg.14 compute**：行 2/18/19/21（不含 μ）；SIM 单 launch **完成**；CPU 三 launch **部分**（û/u） | 部分 | ✓ **~125k** |
| [**pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/**](pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/) | **Alg.14 tail**：行 20 μ_embed + 行 22–24 pack→`c`（分组 ByteEncode；SIM **56259** tick） | ✓ | ✓ |
| [**pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/**](pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) | **Alg.14 行 2/16–24**（prep 外素材→c=c₁‖c₂）；SIM **1 launch** **154781** tick；CPU 4 launch | ✓ | ✓ |
| [**pass-fix-f203-alg14-pke-encrypt-device-k4/**](pass-fix-f203-alg14-pke-encrypt-device-k4/) | **Alg.14 完整 K-PKE.Encrypt**（prep + compute+tail 集成；行 1–22 全设备；**in ek+m+coins → out 仅密文 c**；u/v 不落盘；SEED_D=20260619）；SIM **2 launch 626139** tick | ✓ | ✓ |
| [**pass-fix-f203-alg15-pke-decrypt-device-k4/**](pass-fix-f203-alg15-pke-decrypt-device-k4/) | **Alg.15 完整 K-PKE.Decrypt**（单 kernel；尾融合；生产 input **仅 dk+c+lut** → out **仅 m**；SIM **~283k**）；**`roundtrip_pke_*` / liboqs 默认 Decrypt**；注释+I/O 收紧 2026-07-09 | ✓ | ✓ |
| [**pass-fix-f203-alg19-kem-keygen-device-k4/**](pass-fix-f203-alg19-kem-keygen-device-k4/) | **Alg.19 KEM KeyGen（device）** — **2 launch**（Alg.16 尾内嵌 stable mmad）；无 vendor；SIM **~713k**；**`scripts/` KeyGen 默认** | ✓ | ✓ |
| [**pass-fix-f203-alg20-kem-encaps-device-k4/**](pass-fix-f203-alg20-kem-encaps-device-k4/) | **Alg.20 KEM Encaps（device）** — prep H/G + stable Encrypt；无 vendor；SIM **721010**；**`scripts/` Encaps 默认** | ✓ | ✓ |
| [**pass-fix-f203-alg21-kem-decaps-device-k4/**](pass-fix-f203-alg21-kem-decaps-device-k4/) | **Alg.21 KEM Decaps（device / 交付）** — Decrypt fused + Encrypt + FO；无 vendor；单库+1-session；SIM D**286803**+E**745925**；`scripts/` `DECAPS_DIR` 默认 **stable（无 `-ct`）**；可覆盖回本探针 | ✓ | ✓ |
| [**pass-fix-f203-alg21-kem-decaps-device-ct-k4/**](pass-fix-f203-alg21-kem-decaps-device-ct-k4/) | **Alg.21 KEM Decaps（device / CT 专题）** — 同上架构；SIM 默认 **`decaps_2session`**；仅 `research/formal-lang-dag`；对应 [`stable-…-decaps-ct-k4`](../examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4/) | ✓ | ✓ |

Phase A 早期 harness 已归档：[`frozen/frozen-f203-ntt-phase-a-fsm/`](frozen/frozen-f203-ntt-phase-a-fsm/)（2026-06-19，任务完成非路线否决）。

**MLKEM NTT + 向量集成**：向量全链路 **[vec-k4-v2](pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)**；**8-poly 批 NTT/INTT** **[pass-fix-f203-stage123-ntt-intt-polyvec8-vec](pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)**；标量对照组 [`frozen-fix-f203-2s1e-alg13-16171820-k4`](frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)（2026-06-19 归档，任务完成）。预研 [`exp-k4`](../examples/incubating/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/)。见 [MLKEM-NTT-向量与标量实现指南.md](../docs/notes/MLKEM-NTT-向量与标量实现指南.md)。

---

## 规划中

| 目录 | 说明 |
|------|------|
| **vec-k4-v3**（暂定） | **已关闭（T13b）**：stable KeyGen 已覆盖；勿再开 |

~~`fix-f203-alg19/20/21-*-correctness-k4`~~ — **2026-07-20 冻结** → [`frozen/INDEX.md`](frozen/INDEX.md)（正确性路标任务完成；继任 stable + pass-fix device；**禁止翻源码**）。

**命名**：`pass-fix-*-device-k4` = 去 vendor 设备主线。**Alg.19/20/21 device 均已 `pass-fix-…`**。KEM `scripts/`：KeyGen→`pass-fix-…-keygen-device-k4`；Encaps/Decaps→对应 **stable**（`DECAPS_DIR=`/`ENCAPS_DIR=` 可覆盖回 pass-fix）。**禁止**默认或文档链指回已冻结的 `*-correctness-k4`。

**更名防幽灵（强制）**：`git mv` / 目录晋级 `fix-`→`pass-fix-` **只搬已跟踪文件**；旧路径下 `build_*`/`input`/`output`/`sim_log` 等**未跟踪产物会留成空壳目录**。更名后须 `rm -rf` 旧目录残留，或跑 [`scripts/cleanup-ascendc-test-ghosts.sh`](../scripts/cleanup-ascendc-test-ghosts.sh)。**禁止**再建：`fix-f203-alg21-kem-decaps-device-k4`（已更名）、`pass-probe-*`（误名）、以及已冻结的 `fix-f203-alg{19,20,21}-*-correctness-k4` 活跃路径。正确 Decaps device 仅 [`pass-fix-f203-alg21-kem-decaps-device-k4`](pass-fix-f203-alg21-kem-decaps-device-k4/)。

**KEM 端到端测试（仓库级 `scripts/`，镜像 PKE）**：`liboqs_kem_vs_ascendc.sh`（KeyGen→Encaps→Decaps→reject 四阶段逐级对 liboqs fixture）；**纯 device round-trip（分项，各跑一次，CPU/SIM 分开）**：`roundtrip_kem_keygen.sh` → `roundtrip_kem_encaps.sh` → `roundtrip_kem_decaps.sh`（stash `output/roundtrip_kem/<cpu|sim>/`）；一体入口 `roundtrip_kem_keygen_encaps_decaps.sh`（含拒绝路径）。SIM Decaps 默认 1-session，全链约数分钟/段。

**KEM 分项 kat（固定 stash 密钥 + 每轮随机量，逐字节对 liboqs）**：`liboqs_kem_keygen_batch.sh`（`KEM_KG_EXT_SEED` 同 64B 种子）· `liboqs_kem_encaps_batch.sh`（默认 **device-k4**，`M_FILE` 喂 `m`，默认 **CPU×10+SIM×3**）· `liboqs_kem_decaps_batch.sh`（liboqs `encaps_derand` 造 `c`）；密钥经 `kem_keypair_stash_bootstrap.sh` 落 `output/kem_keypair_stash/`。
| ~~fix-f203-alg14-encrypt-2launch-k4~~ | **已冻结** → [`frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/`](frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/)（家里 agent `27cc93b`，办公室未复验） |
| ~~fix-f203-alg14-pke-encrypt-correctness-k4~~ | **已冻结**（2026-07-10）→ [`frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/`](frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/)；交付 [`stable-fips203-mlkem-pke-encrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) |
| ~~fix-f203-alg15-pke-decrypt-correctness-k4~~ | **已冻结**（2026-07-10）→ [`frozen/frozen-fix-f203-alg15-pke-decrypt-correctness-k4/`](frozen/frozen-fix-f203-alg15-pke-decrypt-correctness-k4/)；交付 [`stable-fips203-mlkem-pke-decrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) |

**ByteEncode₁₂（KeyGen）**：[`pass-fix-f203-2s1e-byteencode12-vec-k4`](pass-fix-f203-2s1e-byteencode12-vec-k4/)（**d=12**）。Encrypt/Decrypt 单算子 **d=4/10** 见上表 **`pass-f203-*-d4-d10-vec-k4`**；ml_kem_1024 的 **d=5/d=11** 在全链 Encrypt/Decrypt 探针内联。

---

## 已关闭路线（frozen）

Alg.13 **Phase A 全链 benchmark**（2026-06-28）、**host-scalar / se-device-scalar**（2026-06-26）、merged_kyber、Alg.13 sepair、Tag5T polybatch-s123、**basemul-vec spike**、**vec-k4 v1**、**innerproduct 二期 half** 等已迁入 [frozen/INDEX.md](frozen/INDEX.md)。**只读 `FROZEN.md` 知悉归档原因**；禁止抄 frozen 实现。活跃 MLKEM 向量见上表 **vec-k4-v2**。
