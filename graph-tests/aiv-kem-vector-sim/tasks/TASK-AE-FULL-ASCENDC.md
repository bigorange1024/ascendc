# TASK · AE 全 AscendC（消 Host Â/t̂/采样）

> 主控下发 · 2026-09-12 · **SIM-only** · 禁 git 分支/commit/push

## 目标

单 AIV、单 launch，**全部密码学步骤在 AscendC 设备核内完成**（允许 Host 只喂公钥/消息/随机币 + NTT 常量表 + workspace GM）。

| 用例 | 纯设备输入契约 | 权威对拍 |
|------|----------------|----------|
| `AE-E-encrypt/` | `ek[1568] \| m[32] \| coins[32]` + roots/map/gamma 表 | `c` ≡ liboqs max=0 |
| `AE-P-encaps/` | `ek[1568] \| m[32]` + 同上表 | `c`/`K` ≡ liboqs max=0 |

## 必须设备化（本刀）

1. **ByteDecode₁₂(t̂)**：`ek[0:1536]` → `t̂[4×256] int32`（可 `#include library/shared/f203_byte_codec/byte_decode12_vec.hpp`）
2. **SampleNTT(Â)×16**：`ρ = ek[1536:1568]`，对 `(i,j)∈[0,4)²`：`SHAKE128(ρ‖i‖j)` + Alg.7 拒绝采样 → `Â[16×256]`（写 workspace GM）
3. **Encrypt 路径 CBD**：`coins` → SHAKE256+CBD η=2 → y/e₁/e₂（Encaps 已有，Encrypt 须对齐，禁止再 Host 预喂 y/e）
4. Encaps 保留已有 **DEVICE_FO + DEVICE_CBD**，再接 1+2

## 硬约束

- `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`，单 launch
- 禁 Cube / CrossCore / GATE 胖 MIX
- 禁抄 `ascendc-tests/frozen/**`、`examples/frozen/**` 源码
- 可复用：`library/shared/keccak_f1600_kernel`（已有 Shake256/Sha3；缺 Shake128 则在该头**同风格补**并写中文注释）、`f203_byte_codec`
- NTT/INTT/matvec 保持现有向量路径
- 自研代码中文注释同轮
- 改 AscendC API 前查 `library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`，缺则查 PDF 并写回
- Host `gen_data.py`：**不得**再写 `ahat.bin`/`that.bin`/`y.bin`/`e1.bin`/`e2.bin` 作为核输入（可留 debug 对照文件但 main 不得 ReadFile 喂核）
- workspace：`ahat`/`that`/`y`/`e1`/`e2`/`yhat` 由 Host `GmAlloc`，**内容由设备写**
- 验收：同目录
  ```bash
  bash run.sh -r cpu -v Ascend910B4
  SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
  ```
  用例根无 stray dump；sync_audit 红线 0

## 假绿三问（对拍失败时先答）

1. golden 是否与实现同源？
2. 权威交叉（liboqs）是否已跑？
3. 跨核读写 Sync？（本核 AIV-only）

## 交付物

- 改后的 `aiv_encrypt.cpp` / `aiv_encaps.cpp` / `main.cpp` / `scripts/gen_data.py` / CMake include
- 更新两用例 `STATUS.md`、战役 `QUEUE.md`
- 日志放到 `/opt/cursor/artifacts/aiv-kem-vector-sim/AE-*-fullascendc-*.log`
- 回报：diff 要点、CPU/SIM maxdiff、tick、仍阻塞点（若有）

## 禁止

- git checkout -b / commit / push / 开 PR
- `-r npu`
- 并行多路 SIM
