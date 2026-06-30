# qa — 遗留事项总表

跨会话跟踪未关闭事项。刷新时须同步：**当日** `qa/YYYY-MM/YYYY-MM-DD-….md`（同日仅一篇，追加章节）+ **`qa/YYYY-MM/INDEX.md`** + **本文件**。

**最近刷新**：2026-06-30（Encrypt G5 PASS · 2launch 探针冻结）

---

## 打开项

| ID | 事项 | 来源 | 状态 |
|----|------|------|------|
| **T14** | **Encrypt G5**：原探针 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) CPU+SIM c.bin max=0（at_r5 + 全 device G4）；家里 `2launch-k4` 已冻结（办公室未复验） | [STATUS](../ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) · [frozen FROZEN](../ascendc-tests/frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/FROZEN.md) | **✅ G5 PASS**；后继：stable 晋级 / KAT 扩展 |
| T1 | Rule/Skill 与目录结构落地 | [2026-06-08 纪要](2026-06/2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md) §一 | **已完成** |
| T2 | **ML-KEM-1024 PKE.KeyGen（Alg.13）** 功能原型：AscendC NTT + 嵌入 C（内积/编解码） | 同上 §六 | **进行中**（**全链 exp + pass 探针 CPU/SIM/KAT ✓**；Alg.16 / NPU 未做） |
| T2a | 写 `docs/specs/fips203-mlkem1024-keygen-plan.md` | §6.5 | 待开工 |
| T2b | 写 `docs/specs/fips203-baseline-registry.md` 初稿（liboqs + 算子级登记） | §6.5 | 待开工 |
| T2c | 确认并建 `examples/incubating/exp-mlkem1024-pke-keygen/` | §6.6 | **已由 [`exp-mlkem-f203-pke-keygen-k4`](../examples/incubating/exp-mlkem-f203-pke-keygen-k4/) 承担**（ML-KEM-768 k=4 交付示例） |
| T2d | D1：固定 seed、全 C 参考 / liboqs golden、bin 布局 | §6.4 | **✓**（`keygen_golden.py` + `kat_liboqs_vs_ascendc.sh` 10 CPU + 1 SIM） |
| T2e | D2：AscendC 16–17、集成 18+encode、cpu/sim（+npu 若有服务器） | §6.4 | **cpu/sim/KAT ✓**（[`exp-mlkem-f203-pke-keygen-k4`](../examples/incubating/exp-mlkem-f203-pke-keygen-k4/) 全链 + [`pass-fix keygen`](../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/)）；**npu 未测** |
| T3 | 他人 AscendC 代码引入流程与目录约定 | §二 | 待定 |
| T4 | 换机安装 `libssl-dev` 后可选重编 liboqs（默认 OpenSSL） | §四 | 可选 |
| T5 | `baseline-registry` 登记 liboqs / ntt_study 算子 API（与 T2b 合并推进） | §四、§六 | 进行中（随 T2b） |
| T6 | **ML-KEM.KeyGen（Alg.16）** — PKE.KeyGen 稳定后再做 | §6.1 | 排队 |
| T7 | FIPS 204 / ML-DSA 路线（Encaps/Decaps/Sign 等） | §二 | 后阶段 |
| T8 | 今日拍板：验收口径、NTT 批/分 launch、SHA3 方案、npu 分工 | §6.6 | **2026-06-09 讨论** |
| T9 | CANN PDF 查阅索引维护 | [2026-06-09 纪要](2026-06/2026-06-09-AscendC平台与CANN文档索引.md) §三 | **已建索引，持续追加** |
| T11 | **Alg.13 集成（2s1e）**：[`vec-k4-v2`](../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) **77958 tick** + [SIM 表](../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md)；[`exp-k4`](../examples/incubating/exp-mlkem-f203-alg13-16171820-2s1e-k4/) FIPS CBD **77996 tick**（prefetch 已同步） | [2026-06-19 纪要 §8–§9](2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md) | stable / npu / `Â` 扩展待做 |
| T10 | **行 18 内积 lazy reduction（仅方案，暂不实现）** | [2026-06-12 纪要](2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md) §5–§6 | **待用户思考** |
| T12 | **exp-k4 增强（非阻塞）**：`liboqs` 同 `SEED_D` 系数对照脚本；mixPass 分段 tick profiling；NPU 实机 | [2026-06-18 纪要](2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md) §6.4 | 打开 |
| T13 | **设备 Alg.13 行 8–15（$s$/$e$，SHAKE256）** — **两阶段** | presample [`INTEGRATION.md`](../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/INTEGRATION.md) | **一a/一b 已冻结归档（2026-06-26）；阶段二 vec-k4-v3 待开工** |
| T13a | **阶段一a 标量正确性** | T13 | **已冻结** → [`frozen-fix-f203-alg13-se-device-scalar-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-se-device-scalar-k4/)（2026-06-26） |
| T13a-v | **阶段一b 设备预采样**：[`pass-fix-f203-alg13-lines8-15-se-k4`](../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/) | T13 | ✅ **PASS**（V3 **133153** + 链式 8–17 **~177553**）；[INTEGRATION.md](../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/INTEGRATION.md) |
| T13a-c | **链式探针 8–17**：[`CHAIN_NTT17.md`](../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/CHAIN_NTT17.md) | T13 | ✅ 合入 T13a-v（同上） |
| T13c | **Alg.7 单 poly SampleNTT**：[`pass-fix-f203-alg7-sample-ntt-k4`](../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/) | T13 | ✅ **功能完成**（CPU/SIM `a_hat` PASS；R0–R4）；SIM 全链 **~80100** tick；[note](../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md) |
| T13d | **R5 向量 compact 独立探针**（8-lane 掩码 + LUT；Compare/Compares 读法） | [2026-06-24 纪要](2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md) | **暂停**；生产用标量 compact |
| T13e | **alg7 微优化**（去重 DataCopy、ROM DataCopy Init、可选仅写 â GM） | 同上 §4 | 可选；期望有限 |
| T13f | **alg7 tick：504B+lazy tail**（若硬指标） | [INTEGRATION_PLAN §1.5](../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md) | 待用户拍板 |
| T13g | **Alg.13 行 3–7（16×`Â`）**：[`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/) | T13c ✅ | **CPU+SIM ✅**；默认 672B **733859**；504B 对照 **549224**（−25%，非默认）；2 AIV **714150**（−2.7%） |
| T13h | **KeyGen prep SIM 双 AIV 并行 Â** | [fix 探针](../ascendc-tests/fix-f203-alg13-device-keygen-k4-dual-aiv/) · [pass 基线](../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | **fix 探针 SIM ✅**（542339 tick）；**liboqs KAT ✅** CPU×10+SIM×1（2026-06-29）；待晋级 pass |
| T13b | **阶段二**：fork `vec-k4-v2` → `vec-k4-v3`，接入 V3 预采样 + 设备 `a_hat` | T13 | **待开工**（依赖 T13a-v ✅、**T13g** 16-poly） |
| T13i | **Phase A 全链 benchmark** | [2026-06-28 纪要 §7.2](2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md#72-phase-a-全链-benchmark-冻结) | **已冻结** → [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/)（2026-06-28） |

---

## 已关闭

| ID | 事项 | 关闭日 |
|----|------|--------|
| — | liboqs 0.15.0 clone + 编译 + ML-KEM/ML-DSA test | 2026-06-08 |
| T11a | vec-k4-v2 阶段 A–D：全链路 UB 融合 + ByteEncode；SIM 86120 tick；v1 冻结 | 2026-06-18 |
| T11b | vec-k4-v2 全工程中文注释 + `IMPLEMENTATION_REFERENCE.md` + 编译修复与复测 | 2026-06-18 |
| T11c | exp-k4 customspec PDF + 【预研】实现（FIPS CBD `gen_data`、fork v2 壳）；CPU/SIM 全链路 PASS（86111 tick） | 2026-06-18 |
| T11d | byteencode12 prefetch **已合入 v2**（77958 全链路）；SIM 单用例表→SIM_BENCHMARK.md | 2026-06-19 |
| T11e | `f203-ntt-phase-a-fsm` **归档 frozen**（任务完成；2s1e/vec-k4-v2 继任，非路线否决） | 2026-06-19 |
| T11f | exp-k4 **ByteEncode prefetch 同步**（customspec § + v2 文件/宏）；CPU/SIM PASS；SIM **77996 tick** | 2026-06-19 |
| T11g | **块紧凑 S0 `[HI_8\|LO_8]` 路线否决**：`frozen-exp-mlkem-sepolyvec8-ntt-k4-block` + `poly8-block-s123` 补强 `FROZEN.md`；A2 1:2 不适合块布局 | 2026-06-19 |
| T11i | **exp KeyGen 自包含交付** | [2026-06-28 纪要 §6](2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md) | **✓ 2026-06-28**（唯一路径、KAT、SIM metrics；[note](../docs/notes/F203-KeyGen-exp交付示例技术总结.md)） |
| T11j | **`backup-project.sh` 恢复与扩展** | 同上 §6.3 | **✓ 2026-06-28**（命名 `v0.1_YYYYMMDDHHMMSS`；见 `backup/v0.1_*` 最新快照） |

---

## 维护

关闭项移至「已关闭」；新增项分配简短 ID。
