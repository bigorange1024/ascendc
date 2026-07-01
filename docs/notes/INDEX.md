# docs/notes — 定稿技术总结

**本目录唯一职责**：原理层技术总结与平台知识库（数学/不变量/可复用模式 + 可选案例附录）。  
**写法**：[技术总结写作模板.md](技术总结写作模板.md)；[docs-archiving.mdc](../../.cursor/rules/docs-archiving.mdc)  
**讨论过程** → `qa/`；**实现方案** → 用例目录 `INTEGRATION_PLAN.md`。

---

## ML-KEM / F203

| 文件 | 说明 |
|------|------|
| [MLKEM-NTT-实现总结.md](MLKEM-NTT-实现总结.md) | Tag5T **数学契约**、poly-batch 约束 |
| [MLKEM-NTT-向量与标量实现指南.md](MLKEM-NTT-向量与标量实现指南.md) | 2s1e **设备实现**、Gather 禁令范围 |
| [F203-ByteEncode12-prefetch技术总结.md](F203-ByteEncode12-prefetch技术总结.md) | ByteEncode₁₂ **prefetch**、Alg.13 输入契约、encode-only tick |
| [F203-CBD-eta2-性能优化技术总结.md](F203-CBD-eta2-性能优化技术总结.md) | Alg.8 CBD η=2 **P0–P2**；锚点 [`pass-fix-f203-alg8-cbd-eta2-k4`](../../ascendc-tests/pass-fix-f203-alg8-cbd-eta2-k4/) |
| [F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md) | NTT+内积 UB 融合；§6 tick 含 **77958** 全链路 |
| [F203-innerproduct-k4-技术总结.md](F203-innerproduct-k4-技术总结.md) | NTT 域内积 4×4×1、布局契约 |
| [F203-merged-kyber-MIX路线技术总结.md](F203-merged-kyber-MIX路线技术总结.md) | merged_kyber MIX（**frozen**，原理） |
| [merged-kyber-poly-batch-NTT技术总结.md](merged-kyber-poly-batch-NTT技术总结.md) | poly-batch NTT、单 TPipe |
| [NTT-Matmul路线废弃说明.md](NTT-Matmul路线废弃说明.md) | NTT 内 `Matmul<>` 判决 |
| [F203-Alg7-SampleNTT-单poly技术总结.md](F203-Alg7-SampleNTT-单poly技术总结.md) | **Alg.7 SampleNTT**（[`pass-fix-f203-alg7-sample-ntt-k4`](../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/)）：单 poly `â[256]`、672B XOF、rej、~80k tick |
| [F203-Alg7-PhaseA-向量化技术总结.md](F203-Alg7-PhaseA-向量化技术总结.md) | Phase A **全链 benchmark**（已冻结 [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/)）：A-v4 反模式、tick 表 |
| [F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md](F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) | KeyGen prep **双 AIV**、SHAKE **ProcessInline**；§4.1 **CPU SUCCESS / 2AIC+4AIV 误读** |
| [F203-KeyGen-prep-Pipe细同步技术总结.md](F203-KeyGen-prep-Pipe细同步技术总结.md) | prep **PipeBarrier** 窄化（Opt-5 CBD 合入） |
| [F203-KeyGen-exp交付示例技术总结.md](F203-KeyGen-exp交付示例技术总结.md) | **exp-mlkem-f203-pke-keygen-k4** 自包含交付、唯一路径、KAT/SIM 验收 |
| [F203-Alg15-Decrypt-2launch编排技术总结.md](F203-Alg15-Decrypt-2launch编排技术总结.md) | **Alg.15 Decrypt** 2-launch 切分；prep/NTT 分离、NTT/INTT 分 kernel；[`fix-f203-alg15-pke-decrypt-correctness-k4`](../../ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/) |
| [F203-PKE-liboqs交叉验证与Compress定点技术总结.md](F203-PKE-liboqs交叉验证与Compress定点技术总结.md) | **PKE L2 liboqs 三阶段** oracle、`Compress_d` 定点 bias 契约（d=5 `(1<<26)`）；[`liboqs_pke_vs_ascendc.sh`](../../scripts/liboqs_pke_vs_ascendc.sh) |
| [F203-KEM-Alg19-KeyGen设备全链技术总结.md](F203-KEM-Alg19-KeyGen设备全链技术总结.md) | **Alg.19 KEM KeyGen** 随机性契约、KeyGen_internal 增量、3168B dk 契约、vendor PKE + 设备 SHA3；[`fix-f203-alg19-kem-keygen-k4`](../../ascendc-tests/fix-f203-alg19-kem-keygen-k4/) |

## AscendC 平台

| 文件 | 说明 |
|------|------|
| [ascendc-DataCopy与数据搬运知识库.md](ascendc-DataCopy与数据搬运知识库.md) | MTE、API 谱系、搬运方法论 |
| [ascendc-TQue与Pipe框架知识库.md](ascendc-TQue与Pipe框架知识库.md) | TPipe/TQue/TBuf、event≤8 |
| [AscendC-CAModel-SIM-funckey与单session约束知识库.md](AscendC-CAModel-SIM-funckey与单session约束知识库.md) | **SIM `aclrtLaunchKernel` 507000 病根**（R1 AIV-only `func_key ≤ 4` / R2 单 ACL session + `aclrtSynchronizeStream`）；P1–P6 可复用模式；案例 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../../ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) `at_r5` 落地 |
| [AscendC-多核MatMul-tiling技术总结.md](AscendC-多核MatMul-tiling技术总结.md) | 多 AIC `SetSingleShape`（frozen 探针） |

## 治理与模板

| 文件 | 说明 |
|------|------|
| [研究路线与frozen治理.md](研究路线与frozen治理.md) | frozen 判决书语义；禁止抄码 |
| [技术总结写作模板.md](技术总结写作模板.md) | 新建 note 的结构模板 |

---

## 维护

新增定稿 → 在本表登记；遵循原理优先、案例附录。
