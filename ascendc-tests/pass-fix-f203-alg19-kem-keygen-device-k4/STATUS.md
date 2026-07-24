# STATUS — pass-fix-f203-alg19-kem-keygen-device-k4

FIPS 203 **Algorithm 19 `ML-KEM.KeyGen()`**（**ml_kem_1024 / k=4**）— **2 launch 设备主线**（stable PKE + 内嵌 Alg.16 尾）。

| 项 | 值 |
|---|---|
| **阶段** | **G3/G4 CPU+SIM PASS**（2026-07-10） |
| **Launch** | **2**（`prep` \| `mmad+kem_tail`）；无第 3 launch |
| **PKE 源** | 编译期 [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/)（无 `vendor/`） |
| **SIM tick** | **~713k**（P1 后 3 次均值 **713227**；fork 首期 **700718**；correctness 3-launch **742558**） |
| **P1 工程** | **定案保留**：stable `F203_KEM_KEYGEN_TAIL` 宏 + 本地 `prep/alg7/` ROM（见 §P1 定案） |
| **I/O** | `ek_kem` 1568B · `dk_kem` 3168B；`SEED_D=20260619` |

## P1 定案保留理由（2026-07-10）

相对首期 **fork `mmad_custom_kem.cpp`**，当前方案 SIM tick 约 **+1.8%**，但：

1. **消除 mmad fork**：stable 为唯一实现源，避免 NTT/行21 变更时的手工 merge；
2. **ROM 不污染 stable**：`gen_alg7_*.py` 输出到本探针 `prep/alg7/`；
3. **PKE 零回归**：宏默认 0，stable KeyGen 行为不变；
4. **I/O 不变**：与 correctness `cmp` 完全一致。

详述：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §4.3–§4.5；代码注释见 `kem/*.hpp`、`stable/compute/mmad_custom.cpp`。

## SHA3 替换入口（日后第三方 AscendC）

KEM 尾段仅两处 **SHA3-256**（当前 `F203SeDeviceKeccak::Sha3OneShot`）：

| 文件 | 用途 |
|------|------|
| `kem/f203_kem_kg_derand_ub.hpp` | z 域分离派生 |
| `kem/f203_kem_kg_tail_fuse.hpp` | H(ek) = SHA3-256(ek_pke) |

布局常量不可改：`f203_kem_kg_layout.h`。替换步骤见各文件头「替换指南」与 INTEGRATION_PLAN §4.5。

## 验收（2026-07-10）

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS** ek/dk max=0 |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；tick 均值 **713227**（3 次） |
| vs 历史 correctness | **已冻结** — 勿再 cmp；见 [`FROZEN.md`](../frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/FROZEN.md) | — |
| stable PKE 回归 | `F203_KEM_KEYGEN_TAIL=0`（默认） | CPU **PASS** |

方案：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) · 约束：[`SELF_CONTAINED.md`](SELF_CONTAINED.md)

## 待办

- Encaps/Decaps device 全链（**T19a/b**，下一主线）
- 第三方 AscendC SHA3-256/512 就绪后：统一封装 `kem/f203_kem_sha3.hpp` 并复验
- 领导要求单目录自包含时：copy stable 树进本目录（launch 契约不变）
