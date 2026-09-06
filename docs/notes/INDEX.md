# docs/notes — 定稿技术总结

**本目录唯一职责**：原理层技术总结与平台知识库（数学/不变量/可复用模式 + 可选案例附录）。  
**写法**：[技术总结写作模板.md](技术总结写作模板.md)；归档约定见 Rule [`ascendc-development.mdc`](../../.cursor/rules/ascendc-development.mdc)（`docs/notes` 写作质量）  

**讨论过程** → `qa/`；**实现方案** → 用例目录 `INTEGRATION_PLAN.md`；**未定稿调研** → [`docs/research/`](../research/INDEX.md)。

---

## Encrypt 实机无卡死（主线知识库）

| 文件 | 说明 |
|------|------|
| [Encrypt-实机无卡死-知识库.md](Encrypt-实机无卡死-知识库.md) | **核心问题专属库**（失败优先）；配套 [`rg-encrypt-npu-hangfree`](../rg-encrypt-npu-hangfree.yaml) |

## ML-KEM / F203

| 文件 | 说明 |
|------|------|
| [MLKEM-NTT-实现总结.md](MLKEM-NTT-实现总结.md) | Tag5T **数学契约**、poly-batch 约束 |
| [MLKEM-NTT-向量与标量实现指南.md](MLKEM-NTT-向量与标量实现指南.md) | 2s1e **设备实现**、Gather 禁令范围 |
| [F203-ByteEncode12-prefetch技术总结.md](F203-ByteEncode12-prefetch技术总结.md) | ByteEncode₁₂ **prefetch**、Alg.13 输入契约、encode-only tick |
| [F203-CBD-eta2-性能优化技术总结.md](F203-CBD-eta2-性能优化技术总结.md) | Alg.8 CBD η=2 **P0–P2**；锚点 [`pass-fix-f203-alg8-cbd-eta2-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg8-cbd-eta2-k4/) |
| [F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md) | NTT+内积 UB 融合；§6 tick 含 **77958** 全链路 |
| [F203-innerproduct-k4-技术总结.md](F203-innerproduct-k4-技术总结.md) | NTT 域内积 4×4×1、布局契约 |
| [F203-merged-kyber-MIX路线技术总结.md](F203-merged-kyber-MIX路线技术总结.md) | merged_kyber MIX（**frozen**，原理） |
| [merged-kyber-poly-batch-NTT技术总结.md](merged-kyber-poly-batch-NTT技术总结.md) | poly-batch NTT、单 TPipe |
| [NTT-Matmul路线废弃说明.md](NTT-Matmul路线废弃说明.md) | NTT 内 `Matmul<>` 判决 |
| [F203-Alg7-SampleNTT-单poly技术总结.md](F203-Alg7-SampleNTT-单poly技术总结.md) | **Alg.7 SampleNTT**（[`pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg7-sample-ntt-k4/)）：单 poly `â[256]`、672B XOF、rej、~80k tick |
| [F203-Alg7-PhaseA-向量化技术总结.md](F203-Alg7-PhaseA-向量化技术总结.md) | Phase A **全链 benchmark**（已冻结 [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/)）：A-v4 反模式、tick 表 |
| [F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md](F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) | KeyGen prep **双 AIV**、SHAKE **ProcessInline**；§4.1 **CPU SUCCESS / 2AIC+4AIV 误读** |
| [F203-KeyGen-prep-Pipe细同步技术总结.md](F203-KeyGen-prep-Pipe细同步技术总结.md) | prep **PipeBarrier** 窄化（Opt-5 CBD 合入） |
| [F203-KeyGen-exp交付示例技术总结.md](F203-KeyGen-exp交付示例技术总结.md) | **exp-fips203-mlkem-pke-keygen-k4** 自包含交付、唯一路径、KAT/SIM 验收 |
| [F203-Alg15-Decrypt-2launch编排技术总结.md](F203-Alg15-Decrypt-2launch编排技术总结.md) | **Alg.15 Decrypt** 2-launch 正确性切分原理；**生产已是 1-kernel**（[`pass-fix-…-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/) / [`exp-…-decrypt-k4`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4/)）；文首 2026-07-09 注 |
| [F203-Compress-Decompress-向量实现指南.md](F203-Compress-Decompress-向量实现指南.md) | **Compress/Decompress_d** 向量路径（Barrett d=4/5、cast_div d=10/11、Decompress 统一 int32）；探针 `pass-f203-*-d-vec-k4` |
| [F203-Compress-Decompress-统一整数舍入技术总结.md](F203-Compress-Decompress-统一整数舍入技术总结.md) | **`2^37/q` 统一 C + `y=2^(37-d)` 消分母**；CT 友好、全 d 纯 int 向量；**§8 业界对比与选型** |
| [F203-Alg14-Encrypt-compute-tail-PASS技术总结.md](F203-Alg14-Encrypt-compute-tail-PASS技术总结.md) | **Alg.14 行 2/16–24** compute+pack **PASS** 探针；c=c₁‖c₂；SIM 1 launch；全链 Encrypt 基线 |
| [F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md](F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md) | **Encrypt 交付权重**：无 NPU 时 **SIM 主参考**、CPU 仅辅助；stable 门禁与 `golden_v` 边界 |
| [F203-ByteEncode-ByteDecode-d-向量与标量选型.md](F203-ByteEncode-ByteDecode-d-向量与标量选型.md) | **ByteEncode/ByteDecode_d** 宏分层（VEC=0/1/2）、tail **Compress 向量 + Encode 标量 pack**、VEC=2 实验不采纳、Decrypt 链 |
| [F203-PKE-liboqs交叉验证与Compress定点技术总结.md](F203-PKE-liboqs交叉验证与Compress定点技术总结.md) | **PKE L2 liboqs** oracle、`Compress_d` 定点 bias（d=5 `(1<<26)`） |
| [F203-KEM-Alg19-KeyGen设备全链技术总结.md](F203-KEM-Alg19-KeyGen设备全链技术总结.md) | **Alg.19 KEM KeyGen**；device [`pass-fix-…-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4/)；历史 correctness **已冻结**（[`FROZEN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/FROZEN.md)） |
| [F203-KEM-Alg20-Encaps设备全链技术总结.md](F203-KEM-Alg20-Encaps设备全链技术总结.md) | **Alg.20 KEM Encaps**；stable [`stable-…-kem-encaps-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/) · device [`pass-fix-…-encaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/) |
| [F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md](F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) | **Alg.21 KEM Decaps**；交付 device [`pass-fix-…-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/) · CT 专题 [`pass-fix-…-device-ct-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-ct-k4/)；历史 correctness **已冻结**（[`FROZEN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg21-kem-decaps-correctness-k4/FROZEN.md)） |
| [F203-Encrypt-compute-行18-19-UB驻留技术总结.md](F203-Encrypt-compute-行18-19-UB驻留技术总结.md) | **Alg.14 compute** 内积→INTT **UB 驻留**、MIX GATE 握手、SIM 标量/MTE 可见性；[`pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/) |

## AscendC 平台

| 文件 | 说明 |
|------|------|
| [ascendc-DataCopy与数据搬运知识库.md](ascendc-DataCopy与数据搬运知识库.md) | MTE、API 谱系、搬运方法论 |
| [ascendc-TQue与Pipe框架知识库.md](ascendc-TQue与Pipe框架知识库.md) | TPipe/TQue/TBuf、event≤8 |
| [AscendC-CAModel-SIM-funckey与单session约束知识库.md](AscendC-CAModel-SIM-funckey与单session约束知识库.md) | **SIM `aclrtLaunchKernel` 507000 病根**（R1 AIV-only `func_key ≤ 4` / R2 单 ACL session + `aclrtSynchronizeStream`）；P1–P6 可复用模式；案例 [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) `at_r5` 落地 |
| [AscendC-CPU与SIM实现分叉开发指南.md](AscendC-CPU与SIM实现分叉开发指南.md) | **强制** `ASCENDC_BUILD_CPU/SIM` + `ASCENDC_SIM_HOST_MODE` 全仓登记；§4.1 新代码写法；附录 A/B 案例 |
| [AscendC-多核MatMul-tiling技术总结.md](AscendC-多核MatMul-tiling技术总结.md) | 多 AIC `SetSingleShape`（frozen 探针） |
| [AscendC多环境运行纪要.md](AscendC多环境运行纪要.md) | **WSL / Cloud / 真机**：三档 cpu|sim|npu、`runtime_env`、Clang `-Werror`、SIM dump 分轨 |

## 治理与模板

| 文件 | 说明 |
|------|------|
| [研究路线与frozen治理.md](研究路线与frozen治理.md) | frozen 判决书语义；禁止抄码 |
| [技术总结写作模板.md](技术总结写作模板.md) | 新建 note 的结构模板 |

---

## 维护

新增定稿 → 在本表登记；遵循原理优先、案例附录。
