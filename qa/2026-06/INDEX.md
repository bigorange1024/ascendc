- [2026-06-26 标量探针冻结](2026-06-26-标量探针冻结.md)
# qa/2026-06 — 2026 年 6 月讨论索引

当月讨论摘要与遗留快照；细节见各日 `.md`。总表见 [../TODO.md](../TODO.md)。

---

## 讨论

| 日期 | 文件 | 关键字 |
|------|------|--------|
| 2026-06-30 | [2026-06-30-funckey-507000本地独立验证.md](2026-06-30-funckey-507000本地独立验证.md) | **funckey ≥ 5 → 507000** · Encrypt G5 · Decrypt G4 · **`roundtrip_pke_*` 闭环** · **`liboqs_pke_vs_ascendc` 三阶段 CPU+SIM max=0** · **`Compress_5` `(1<<26)` 修复** · T14b/T15/T16 关闭 |
| 2026-06-30 | [2026-06-30-Encrypt单session重建与SIM-funckey5-507000病根.md](2026-06-30-Encrypt单session重建与SIM-funckey5-507000病根.md) | **（已冻结探针）** 家里 agent 单 session 重建 · R1/R2 受控实验 · commit `27cc93b` |
| 2026-06-29 | [2026-06-29-KeyGen双AIV并行fork探针.md](2026-06-29-KeyGen双AIV并行fork探针.md) | **fix-dual-aiv** · KAT ✅ · **Encrypt G5 CPU ✅ SIM c 阻塞** · G3 审计 §9.9 |
| 2026-06-28 | [2026-06-28-KeyGen探针pass前缀与生产IO.md](2026-06-28-KeyGen探针pass前缀与生产IO.md) | **pass-fix keygen** · **pass polyvec8 NTT/INTT** · **exp 自包含** · SIM **~884532** · **backup-project.sh 刷新** |
| 2026-06-26 | [2026-06-26-标量探针冻结.md](2026-06-26-标量探针冻结.md) | **标量 frozen** · 历史 workaround（已 superseded）· [2026-06-29 KeyGen](2026-06-29-KeyGen双AIV并行fork探针.md) |
| 2026-06-25 | [2026-06-25-KeyGen-prep优化路线图.md](2026-06-25-KeyGen-prep优化路线图.md) | **KeyGen Step4** · prep **454170** · Opt-2/4 ✅ · Opt-1/3 **已关闭** · Opt-3 备份 `keygen-opt3-pre_20260625185315` |
| 2026-06-24 | [2026-06-24-Alg7单poly验收与R5向量compact.md](2026-06-24-Alg7单poly验收与R5向量compact.md) | **单 poly 验收** · **16-poly Â §7** · **504B −25% §7.3** · **KeyGen G0–G4 §8** · [单 poly note](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md) |
| 2026-06-23 | [2026-06-23-SampleNTT-PhaseA向量化讨论.md](2026-06-23-SampleNTT-PhaseA向量化讨论.md) | **Alg.7 SampleNTT** · **672B §16** · **504 vs 672 tick §17** · **rej 剔除双方案 §18** · [`pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/) |
| 2026-06-22 | [2026-06-22-Alg8-CBD-eta2-性能优化讨论.md](2026-06-22-Alg8-CBD-eta2-性能优化讨论.md) | **CBD 单用例 P0–P2** · SWAR/LUT · DataCopy · 双 AIV · **Pipe 细同步暂停** |
| 2026-06-19 | [2026-06-19-ByteEncode12-only探针与prefetch实验.md](2026-06-19-ByteEncode12-only探针与prefetch实验.md) | **prefetch v2** · **块 S0 否决 §10** · **设备 PRF §12–§14（两阶段）** |
| 2026-06-18 | [2026-06-18-内积布局与NTT内积UB融合讨论.md](2026-06-18-内积布局与NTT内积UB融合讨论.md) | **`a_hat` 行主序** · UB 融合 **86120 tick**（tile32 encode，已被 prefetch 取代） · v1 冻结 |
| 2026-06-17 | [2026-06-17-innerproduct-k4一二期路线讨论.md](2026-06-17-innerproduct-k4一二期路线讨论.md) | polyvec 内积 2×2×1 · **二期 half 冻结** · 一期全 poly 交付 · `frozen-...-halfbatch` |
| 2026-06-16 | [2026-06-16-Alg11-12向量化与微优化A-B.md](2026-06-16-Alg11-12向量化与微优化A-B.md) | Alg.11/12 · Barrett · **`ALG11_MEM_OPS` ROM DataCopy（−45% tick）** · `ALG11_VEC_OPTS` A/B · `pass-fix-f203-alg11-12-multiplyntts-k4` |
| 2026-06-15 | [2026-06-15-ByteEncode12向量与Scatter讨论.md](2026-06-15-ByteEncode12向量与Scatter讨论.md) | ByteEncode · Scatter · **basemul spike §8** · `frozen-fix-f203-2s1e-basemul-vec-k4`（2026-06-16 冻结） |
| 2026-06-12 | [2026-06-12-F203-alg13行18-TQue与模运算讨论.md](2026-06-12-F203-alg13行18-TQue与模运算讨论.md) | alg13 行 18 · TQue/mod · poly-batch · se_pair §10 · **行 19–20 + UB 驻留 §11** · `fix-f203-alg13-161718-polybatch-sepair-k4` |
| 2026-06-11 | [2026-06-11-ascendc-engineering-notes与数据搬运.md](2026-06-11-ascendc-engineering-notes与数据搬运.md) | engineering-notes · DataCopy 知识库 · exp-int8 多核 tiling · NTT `Matmul<>` **废弃冻结** · CPU/SIM 同步 |
| 2026-06-10 | [2026-06-10-F203-MIX-merged_kyber路线与limb6.md](2026-06-10-F203-MIX-merged_kyber路线与limb6.md) | merged_kyber MIX 壳 · limb6 探针 · 融合模板暂缓（6/11 澄清非永久否决） |
| 2026-06-09 | [2026-06-09-AscendC平台与CANN文档索引.md](2026-06-09-AscendC平台与CANN文档索引.md) | exp-sepolyvec8-ntt-k8 方案 PDF §八 · KernelLaunch · int8 tile 16×32×16 |
| 2026-06-08 | [2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md](2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md) | Rule/Skill · ML-KEM1024 KeyGen · Alg13→16 · AscendC+嵌C · 明日开工 |

---

## 当月遗留（快照）

- **8-poly 批 NTT/INTT（AscendC 向量）**：[`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) NTT SIM **30347** / INTT **30340**；1 AI Core · 1 launch — [note](../../docs/notes/F203-polyvec8-stage123-NTT-INTT技术总结.md)
- **KeyGen 交付**：探针 [`pass-fix-f203-alg13-device-keygen-k4`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) SIM **~886801**；示例 [`exp-mlkem-f203-pke-keygen-k4`](../../examples/incubating/exp-mlkem-f203-pke-keygen-k4/) **自包含唯一路径** SIM **~884532** + liboqs KAT — [note](../../docs/notes/F203-KeyGen-exp交付示例技术总结.md)
- **ByteEncode prefetch**：已合入 v2；全链路 **77958 tick**；单用例表→[SIM_BENCHMARK.md](../../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md)
- **Alg.13 行 16–20（2s1e UB）**：探针 [`vec-k4-v2`](../../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) **77958** + 预研 [`exp-k4`](../../examples/incubating/exp-mlkem-f203-alg13-16171820-2s1e-k4/) **77996**（FIPS CBD，prefetch 已同步）；NPU / stable 晋级待做
- **平台基线**：CANN 9.0.0、Atlas A2（910B4）、`blockDim=1` MIX AIC_1_2
- **Alg.7 SampleNTT**：[`pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/) **R0–R4 ✅**
- **Alg.13 行 3–7（Â）**：[`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/) CPU+SIM ✅
- **Alg.13 行 8–15（s/e）**：[`pass-fix-f203-alg13-lines8-15-se-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/) ✅
- **Phase A 全链（已冻结）**：[`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) — tick 表只读
- **KeyGen prep 双 AIV 并行 Â**：✅ 完成（pass + stable）；见 [2026-06-29](2026-06-29-KeyGen双AIV并行fork探针.md)
- **CBD 子轨**：[`pass-fix-f203-alg8-cbd-eta2-k4`](../../ascendc-tests/pass-fix-f203-alg8-cbd-eta2-k4/) **`pass-` 终态** CPU+SIM ✅；P2 SIM **18048** tick；P1b **33311**（KeyGen 生产同构）
- **Encrypt 单 session 全链打通 + SIM 两大病根**：新探针 `fix-f203-alg14-encrypt-2launch-k4` **CPU+SIM c.bin max=0 ✅**；病根①AIV func_key≥5→507000（压到≤5 个核）②host 拼 matM 前缺 sync→û 全 0；见 [2026-06-30](2026-06-30-Encrypt单session重建与SIM-funckey5-507000病根.md)

---

## 维护

**同一自然日**只维护 **`qa/2026-06/YYYY-MM-DD-<关键词>.md` 一篇**；新话题**追加章节**到当日文件，**禁止**同日再建第二个 `.md`。新日 → 新建一篇。勿在 `qa/` 根目录堆放日纪要。
