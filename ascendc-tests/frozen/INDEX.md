# frozen — 已冻结 / 废弃的 ascendc-tests 探针

**前缀**：`frozen-<原目录名>/`

**语义**：**路线关闭判决**。可**进入**阅读 `FROZEN.md` 等关闭说明；**禁止**将其中源码/路线带入活跃探针。

| Agent / 开发者 | 规则 |
|----------------|------|
| **进门** | 读 `FROZEN.md`、`STATUS.md`、本 `INDEX.md`、`qa/` — 知悉为何不采纳、继任探针 |
| **出门** | 禁止复制/移植/fork 到活跃目录；禁止标为「权威/上游」；禁止跑 CI |

细则见 [.cursor/rules/ascendc-development.mdc](../../.cursor/rules/ascendc-development.mdc) §`**/frozen/`**；研究型仓库说明见 [研究路线与frozen治理.md](../../docs/notes/研究路线与frozen治理.md)。examples 侧 frozen 见 [examples/frozen/INDEX.md](../../examples/frozen/INDEX.md)。

**活跃 MLKEM 探针**：向量全链路 [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)；预研 [`exp-k4`](../../examples/incubating/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/) — 见 [MLKEM-NTT-向量与标量实现指南.md](../../docs/notes/MLKEM-NTT-向量与标量实现指南.md)。标量对照组已归档：[`frozen-fix-f203-2s1e-alg13-16171820-k4`](frozen-fix-f203-2s1e-alg13-16171820-k4/)。



## 2026-07-10 — PKE Encrypt/Decrypt 正确性探针归档（任务完成）

| 目录 | 原角色 | 关闭原因（摘要） | 继任 |
|------|--------|------------------|------|
| [frozen-fix-f203-alg14-pke-encrypt-correctness-k4/](frozen-fix-f203-alg14-pke-encrypt-correctness-k4/) | Alg.14 Encrypt G5 正确性拼装 | **正确性任务完成**；交付已晋级 stable；**仍作** alg20/21 `vendor_sync` G5 源（stable 布局不兼容） | 交付：[`stable-…-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)；KEM vendor：本目录 |
| [frozen-fix-f203-alg15-pke-decrypt-correctness-k4/](frozen-fix-f203-alg15-pke-decrypt-correctness-k4/) | Alg.15 Decrypt G4（2 launch）正确性 | **正确性任务完成**；交付已晋级 stable（1-kernel）；**仍作** alg21 `vendor_sync` G4 源 | 交付：[`stable-…-decrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/)；KEM vendor：本目录 |

## 2026-06-30 — Encrypt 家里分叉探针关闭（办公室未复验）

| 目录 | 原角色 | 关闭原因（摘要） | 继任 |
|------|--------|------------------|------|
| [frozen-fix-f203-alg14-encrypt-2launch-k4/](frozen-fix-f203-alg14-encrypt-2launch-k4/) | 家里 agent 单 session 重建 Encrypt 整树（commit `27cc93b`） | **办公室未复验** PASS；与原探针分叉；R1/R2 结论已在原探针 G5 落地 | [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)（原 G5 正确性探针已同步冻结） |

## 2026-06-29 — KeyGen 串行 Â pass 归档（dual-aiv 晋级）

| 目录 | 原角色 | 归档原因（摘要） | 继任 |
|------|--------|------------------|------|
| [frozen-fix-f203-alg13-device-keygen-k4/](frozen-fix-f203-alg13-device-keygen-k4/) | Alg.13 全链 KeyGen（block0 串行 Â workaround） | SIM **≈886801**；非终态最优 | [`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/) · [`exp-fips203-mlkem-pke-keygen-k4`](../../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/) |

## 2026-06-28 — Phase A 全链 benchmark 归档（任务完成）

| 目录 | 原角色 | 归档原因（摘要） | 继任 |
|------|--------|------------------|------|
| [frozen-fix-f203-alg13-device-presample-a-hat-k4/](frozen-fix-f203-alg13-device-presample-a-hat-k4/) | G+A+P+C 单 launch Phase A（行 3–7+8–15） | tick 实验完成；集成路径已拆分 | [`pass-fix-f203-alg7-sample-ntt-k4`](../pass-fix-f203-alg7-sample-ntt-k4/) · [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../pass-fix-f203-alg13-lines3-7-a-hat-k4/) · [`pass-fix-f203-alg13-lines8-15-se-k4`](../pass-fix-f203-alg13-lines8-15-se-k4/) |

## 2026-06-26 — Alg.13 标量正确性探针（任务完成归档）

| 目录 | 原角色 | 冻结原因（摘要） | 继任 |
|------|--------|------------------|------|
| [frozen-fix-f203-alg13-se-device-scalar-k4/](frozen-fix-f203-alg13-se-device-scalar-k4/) | 设备行 8–15 标量 `SEED_D`→`src` | 阶段一a golden PASS；presample V3 已接班 | [`pass-fix-f203-alg13-lines8-15-se-k4`](../pass-fix-f203-alg13-lines8-15-se-k4/) |
| [frozen-fix-f203-alg13-host-scalar-fullchain-k4/](frozen-fix-f203-alg13-host-scalar-fullchain-k4/) | Host 行 8–20 标量全链 golden | 正确性验证完成；golden 胶水迁 `library/shared` | [`golden_se_sampling.py`](../../library/shared/fips203_se_sample/golden_se_sampling.py)、[`vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) |

## 2026-06-19 — 块紧凑 S0 路线否决

| 目录 | 原角色 | 冻结原因（摘要） |
|------|--------|------------------|
| [frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123/](frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123/) | merged_kyber 块紧凑 poly8 全链路 | **A2 1:2** Stage3 须 GM；Gather 行 `p`/`8+p` 分散；ntt_study 1:1 无差别 **不可外推** |

examples 侧同期否决：[`frozen-exp-mlkem-sepolyvec8-ntt-k4-block`](../../examples/frozen/frozen-exp-mlkem-sepolyvec8-ntt-k4-block/)。**8-poly 紧凑向量终态**：[`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)；历史 exp [`exp-sepolyvec8-ntt-k8`](../../examples/incubating/exp-sepolyvec8-ntt-k8/)；全链路 [`vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)。详见各目录 `FROZEN.md`。

---

## 2026-06-19 — 标量 2s1e-k4 归档（任务完成）

| 目录 | 原角色 | 归档原因（摘要） |
|------|--------|------------------|
| [frozen-fix-f203-2s1e-alg13-16171820-k4/](frozen-fix-f203-2s1e-alg13-16171820-k4/) | Alg.13 行 16–20 **标量全链路**对照组 | **使命完成**；vec-k4-v2 / exp-k4 已接替；**非**路线否决（见 `FROZEN.md`） |

---

## 2026-06-19 — Phase A harness 归档（任务完成）

| 目录 | 原角色 | 归档原因（摘要） |
|------|--------|------------------|
| [frozen-f203-ntt-phase-a-fsm/](frozen-f203-ntt-phase-a-fsm/) | F203 Phase A：6bit encode + MIX CrossCore 早期 harness | **使命完成**；2s1e / vec-k4-v2 全链路已 PASS；**非**路线否决（见 `FROZEN.md`） |

---

## 2026-06-18 — vec-k4 v1 冻结（由 vec-k4-v2 取代）

| 目录 | 原角色 | 冻结原因（摘要） |
|------|--------|------------------|
| [frozen-fix-f203-2s1e-alg13-16171820-vec-k4/](frozen-fix-f203-2s1e-alg13-16171820-vec-k4/) | MLKEM 行 16–20 向量集成 v1 | **half→p→j** 路线；SIM **~137k** 超红线；大 scratch；由 **vec-k4-v2**（j→p UB 融合，**86120** tick 全链路 PASS）取代 |

详见 `FROZEN.md`；纪要 [qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md](../../qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md) §5。

---

## 2026-06-17 — innerproduct 二期 half 批处理冻结

| 目录 | 原角色 | 冻结原因（摘要） |
|------|--------|------------------|
| [frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch/](frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch/) | 2×2×1 内积二期 `j→half→p` | **SIM 无收益**（~23.2k vs 一期打平）；复杂度高；4×4×1 未验证；由 [`pass-fix-f203-alg11-12-innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/) **一期全 poly** 取代 |

详见 `FROZEN.md`；纪要 [qa/2026-06/2026-06-17-innerproduct-k4一二期路线讨论.md](../../qa/2026-06/2026-06-17-innerproduct-k4一二期路线讨论.md)。

---

## 2026-06-16 — basemul-vec spike 冻结

| 目录 | 原角色 | 冻结原因（摘要） |
|------|--------|------------------|
| [frozen-fix-f203-2s1e-basemul-vec-k4/](frozen-fix-f203-2s1e-basemul-vec-k4/) | 2s1e 行 18 basemul B1/B2 | SIM **慢于标量**；混合 Barrett；无 `MEM_OPS=1`；由 [`pass-fix-f203-alg11-12-multiplyntts-k4`](../pass-fix-f203-alg11-12-multiplyntts-k4/) 取代 |

详见 `FROZEN.md`。

---

## 2026-06-15 — F203 四探针冻结（由 2s1e 取代）

下列目录自 `ascendc-tests/` 迁入 `frozen/`；**路线已关闭**，新 MLKEM 集成 **勿 fork、勿抄码**。

| 目录 | 原角色 | 冻结原因（摘要） |
|------|--------|------------------|
| [frozen-fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123/](frozen-fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123/) | NTT-only「权威」 | Gather S3、竖堆 mat_c；由 2s1e 取代 |
| [frozen-fix-f203-tag5t-ntt256-limb6-poly8-planar-s12/](frozen-fix-f203-tag5t-ntt256-limb6-poly8-planar-s12/) | 平面 S1+2 试验 | 半成品；平面已并入 2s1e |
| [frozen-fix-f203-alg13-161718-polybatch-sepair-k4/](frozen-fix-f203-alg13-161718-polybatch-sepair-k4/) | Alg.13 se_pair | peer GM、Gather 草案、路线批评 |
| [frozen-fix-f203-alg13-161718-polybatch-sepair-k4-onelaunch/](frozen-fix-f203-alg13-161718-polybatch-sepair-k4-onelaunch/) | 单趟 launch | CrossCore 脆弱、算力利用差 |

各目录详见 `FROZEN.md`。

---

## 2026-06-12 — MLKEM NTT 探针收敛冻结

下列目录自 `ascendc-tests/` 迁入 `frozen/frozen-<原名>/`；**路线已关闭**，新开发勿 fork、勿抄码。

| 目录 | 原角色 | 冻结原因 |
|------|--------|----------|
| [frozen-fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123/](frozen-fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123/) | Tag5T limb 面对半 Stage3 | 由 **poly-batch** 取代（同 `MlkemNtt`，布局过时） |
| [frozen-fix-f203-alg13-161718-k4/](frozen-fix-f203-alg13-161718-k4/) | Alg.13 行 16–18 | 行 16–17 仍 limbsplit；待基于 polybatch 重建 |
| [frozen-fix-f203-alg13-16171819-k4/](frozen-fix-f203-alg13-16171819-k4/) | Alg.13 行 16–19 + ByteEncode | 同上 |
| [frozen-fix-merged-kyber-ntt256-limb6-poly8-s123/](frozen-fix-merged-kyber-ntt256-limb6-poly8-s123/) | merged_kyber 交错 poly8 全链路 | golden=`ntt_sim_kyber`，**非** FIPS `MlkemNtt` |
| [frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123/](frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123/) | merged_kyber 块紧凑 poly8 | golden≠`MlkemNtt`；**2026-06-19 路线否决**（A2 块布局，见 `FROZEN.md`） |
| [frozen-fix-merged-kyber-ntt256-limb6-poly2-s123/](frozen-fix-merged-kyber-ntt256-limb6-poly2-s123/) | poly2 全链路 | merged_kyber 遗产 |
| [frozen-fix-merged-kyber-ntt256-limb6-poly2-s12/](frozen-fix-merged-kyber-ntt256-limb6-poly2-s12/) | poly2 Stage1+2 | merged_kyber 遗产 |
| [frozen-merged-kyber-ntt256/](frozen-merged-kyber-ntt256/) | Phase D，7bit MIX | merged_kyber 单 poly 基线；golden≠`MlkemNtt` |
| [frozen-merged-kyber-ntt256-limb6/](frozen-merged-kyber-ntt256-limb6/) | Phase D′，6bit 单 poly | 同上；原 F203 fork 基线 |
| [frozen-merged-kyber-ntt256-limb6-poly2-s12/](frozen-merged-kyber-ntt256-limb6-poly2-s12/) | poly2 Stage1+2 only | 同上；`fix-*-poly2-s12` 之上游 |

纪要：[qa/2026-06/2026-06-12-… §7](../../qa/2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md#7-mlkem-ntt-poly-batch-架构定稿同日追加)、[MLKEM-NTT-实现总结.md](../../docs/notes/MLKEM-NTT-实现总结.md)

---

## 2026-06-11 — NTT 内 Matmul 路线冻结

**共同背景**：尝试在 Kyber NTT 全链路中用高阶 `Matmul<>` 或两段式 Matmul 替代 `merged_kyber` 的 `AicMmad`；路线 **废弃中止**。

经验教训：[qa/2026-06/2026-06-11-…#NTT-Matmul](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)

---

## 用例与冻结原因（Matmul 路线）

### [frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s12-matmul/](frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s12-matmul/)

| 项 | 内容 |
|----|------|
| **原路径** | `ascendc-tests/fix-merged-kyber-ntt256-limb6-poly8-block-s12-matmul` |
| **CPU** | ✓ 两段式（Stage1 `mmad` → Stage2 `Matmul<>`），`max_diff=0` |
| **SIM** | ✗ Stage1 ~5s 通过；Stage2 持续 `pem_lsu invalid ldst addr` |
| **冻结类型** | **路线废弃中止**（调研主动停止，非调通后入库） |

**为何冻结**：

1. 目标是在 block-s123 的块紧凑 S0 之后，用 **独立第二趟** `Matmul<>` 做 Stage2，而非沿用已验证的 `AicMmad`。
2. CPU 两段式可对拍，但 SIM 在 Stage2 非法 GM 访问挂死；改用与孤立探针相同的 `int8_matmul_custom` kernel 后 **仍挂** → 根因在 **Host 侧 GM/workspace/tiling 契约**，非 kernel 源码 alone。
3. 双 kernel 混编时 Stage2 还可能走 AIC-only stub（缺 `ffts_addr`），与单 kernel 包行为不一致。
4. 与主路径 `block-s123`（`AicMmad`、SIM ✓）相比，继续投入性价比低；拍板冻结。

**末次日志**：`frozen-fix-merged-kyber-…-s12-matmul/logs/s12-sim-final-20260611.log`

---

### [frozen-int8-matmul-cube-16x256x512/](frozen-int8-matmul-cube-16x256x512/)

| 项 | 内容 |
|----|------|
| **原路径** | `ascendc-tests/int8-matmul-cube-16x256x512` |
| **CPU** | ✓ `16×256×512` 原生 Matmul（无垫片），`max_abs_diff=0` |
| **SIM** | ✓ **仅孤立工程** ~9s；编入 s12-matmul Host 后仍失败 |
| **冻结类型** | **随 NTT Matmul 路线一并废弃**（探针本身孤立 SIM 曾通过） |

**为何冻结**：

1. 为 Kyber Stage2 验证 `C[16,512]=A[16,256]×B[256,512]` 的 `Matmul<>` tiling / launch 参数。
2. 孤立运行时 SIM 通过，说明 **kernel + tiling 在单一 Host 壳下可工作**；但无法闭合「NTT Stage1 产出 `mat_a` → Stage2 Matmul」的多段 Host 集成。
3. 路线已废弃，探针使命结束；保留作「孤立 kernel 曾过、集成不过」的对照。

---

### [frozen-int8-matmul-cube-128x512x512/](frozen-int8-matmul-cube-128x512x512/)

| 项 | 内容 |
|----|------|
| **原路径** | `ascendc-tests/int8-matmul-cube-128x512x512` |
| **CPU** | ✓ 多核 tiling 扫参（1/4/8/16 AIC），`max_abs_diff=0` |
| **SIM** | ✗ 未通过 |
| **冻结类型** | **废弃**（SIM 未闭合 + 随 Matmul 路线停止维护） |

**为何冻结**：

1. 原目标：在 `128×512×512` 垫片形状上扫 `Matmul<>` 多核 `SetSingleShape` / `SetFixSplit` 参数，为 Kyber Stage2 垫片方案服务。
2. Kyber 已改 **原生 `16×256×512`（无垫片）**；垫片扫参与当前 limb6 契约脱节。
3. SIM 从未通过；NTT 内 `Matmul<>` 路线废弃后不再继续扫参。

相关笔记：[qa/2026-06/2026-06-11-…#exp-int8-matmul](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#exp-int8-matmul-多核-tiling-实验)
