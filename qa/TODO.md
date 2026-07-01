# qa — 遗留事项总表

跨会话跟踪未关闭事项。刷新时须同步：**当日** `qa/YYYY-MM/YYYY-MM-DD-….md`（同日仅一篇，追加章节）+ **`qa/YYYY-MM/INDEX.md`** + **本文件**。

**最近刷新**：2026-06-30（关闭 **T15** Decrypt G4、**T16** PKE round-trip；打开项 T13b/T11/T14a/T15a）

---

## 打开项（按优先级）

| 优先级 | ID | 事项 | 状态 |
|--------|-----|------|------|
| **P1** | **T13b** | fork [`vec-k4-v2`](../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) → **vec-k4-v3**，接入 V3 预采样 + 设备 `a_hat` | **待开工**（上游 T13a-v / T13g 均已 PASS） |
| **P2** | **T11** | **2s1e** 探针/exp → [`examples/stable/`](../examples/stable/) 晋级 | 探针 **77958** tick PASS；**stable / NPU** 未做 |
| **P3** | **T14a** | **Encrypt** 探针 G5 → `examples/stable/stable-mlkem-f203-pke-encrypt-k4`（名待定）晋级 | G5 探针 CPU+SIM ✅；**stable 未建** |
| **P4** | **T15a** | **Decrypt** 探针 G4 → `examples/stable/stable-mlkem-f203-pke-decrypt-k4`（名待定）晋级 | G4 探针 CPU+SIM ✅；**stable 未建** |
| P5 | **T14b** | Encrypt **liboqs KAT**（可选；探针当前刻意不做） | 未开工 |
| — | **T2a** | 写 `docs/specs/fips203-mlkem1024-keygen-plan.md` | 待开工 |
| — | **T2b / T5** | `docs/specs/fips203-baseline-registry.md` 初稿 + liboqs/ntt_study API 登记 | 待开工（stable 交付 golden 依赖） |
| — | **T2** | KeyGen **后继**：Alg.16、ML-KEM-1024 规格泛化、**NPU 实机** | k=4 探针+stable **已交付**（见 T13h 关闭） |
| — | **T12** | exp-k4 增强：liboqs 系数对照、mixPass profiling、NPU | 非阻塞 |
| — | **T3** | 他人 AscendC 代码引入流程 | 待定 |
| — | **T4** | 换机后可选重编 liboqs（OpenSSL） | 可选 |
| — | **T6** | ML-KEM **Alg.16** KeyGen | 排队（PKE KeyGen stable 后再做） |
| — | **T7** | FIPS 204 / ML-DSA | 后阶段 |

---

## 暂缓 / 待拍板（非阻塞）

| ID | 事项 | 说明 |
|----|------|------|
| **T10** | 行 18 内积 lazy reduction | 仅方案；[2026-06-12 纪要](2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md) §5–§6 |
| **T13f** | alg7 **504B+lazy tail** tick 优化 | 待用户拍板硬指标 |
| **T13d** | R5 向量 compact 独立探针 | **暂停**；生产用标量 compact |
| **T13e** | alg7 微优化（DataCopy/ROM） | 可选；期望有限 |

---

## 持续维护（非关闭）

| ID | 事项 |
|----|------|
| **T9** | [`CANN-AscendC算子开发接口参考-查阅索引.md`](../library/documents/CANN-AscendC算子开发接口参考-查阅索引.md) — 写 AscendC 前追加查阅记录 |

---

## 已关闭

| ID | 事项 | 关闭日 |
|----|------|--------|
| **T16** | PKE device round-trip：[`scripts/roundtrip_pke_encrypt_decrypt.sh`](../scripts/roundtrip_pke_encrypt_decrypt.sh) KeyGen 密钥 → Encrypt `c.bin` → Decrypt `m.bin`；CPU+SIM **max=0**（32B）；见 qa §16 | 2026-06-30 |
| **T15** | Decrypt G4：[`fix-f203-alg15-pke-decrypt-correctness-k4`](../ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/) CPU+SIM m.bin max=0；**2 launch**（prep \| ntt+intt）；SIM **~427k** tick | 2026-06-30 |
| **T14** | Encrypt G5：[`fix-f203-alg14-pke-encrypt-correctness-k4`](../ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) CPU+SIM c.bin max=0；家里 `2launch-k4` → frozen | 2026-06-30 |
| **T13h** | KeyGen prep **双 AIV 并行 Â**：[`pass-fix-f203-alg13-device-keygen-k4`](../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) SIM **542339** + KAT；晋级 [`stable-mlkem-f203-pke-keygen-k4`](../examples/stable/stable-mlkem-f203-pke-keygen-k4/) | 2026-06-29 |
| **T13g** | Alg.13 行 3–7（16×`Â`）：[`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/) CPU+SIM ✅ | 2026-06-28 |
| **T13c** | Alg.7 单 poly SampleNTT：[`pass-fix-f203-alg7-sample-ntt-k4`](../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/) 功能完成 | 2026-06-24 |
| **T13a-v** | 设备预采样 V3：[`pass-fix-f203-alg13-lines8-15-se-k4`](../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/) PASS | 2026-06-26 |
| **T13a-c** | 链式探针 8–17 | 合入 T13a-v |
| **T13a** | 阶段一a 标量 | → [`frozen-fix-f203-alg13-se-device-scalar-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-se-device-scalar-k4/) |
| **T13i** | Phase A 全链 benchmark | → [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) |
| **T11i** | exp KeyGen 自包含交付 | 2026-06-28 |
| **T11j** | `backup-project.sh` 恢复与扩展 | 2026-06-28 |
| **T2d** | KeyGen D1 golden / liboqs KAT 布局 | 2026-06-28 |
| **T2c** | exp-mlkem1024 目录 | 由 [`exp-mlkem-f203-pke-keygen-k4`](../examples/incubating/exp-mlkem-f203-pke-keygen-k4/) 承担 |
| **T2e** | KeyGen D2 cpu/sim/KAT | 2026-06-29（**NPU 未测** → 并入 T2 打开项） |
| **T8** | 2026-06-09 验收口径 / NTT launch / SHA3 拍板 | 2026-06-09 |
| **T1** | Rule/Skill 与目录结构落地 | 2026-06-08 |
| — | liboqs 0.15.0 clone + 编译 + ML-KEM/ML-DSA test | 2026-06-08 |
| T11a | vec-k4-v2 阶段 A–D UB 融合；v1 冻结 | 2026-06-18 |
| T11b | vec-k4-v2 中文注释 + 编译修复 | 2026-06-18 |
| T11c | exp-k4 customspec + 预研实现 PASS | 2026-06-18 |
| T11d | byteencode12 prefetch 合入 v2（77958 tick） | 2026-06-19 |
| T11e | `f203-ntt-phase-a-fsm` 归档 frozen | 2026-06-19 |
| T11f | exp-k4 ByteEncode prefetch 同步（77996 tick） | 2026-06-19 |
| T11g | 块紧凑 S0 路线否决 | 2026-06-19 |

---

## 维护

关闭项只追加、不删历史行。新增打开项分配简短 ID（T14 后继已用 T14a/T14b）。
