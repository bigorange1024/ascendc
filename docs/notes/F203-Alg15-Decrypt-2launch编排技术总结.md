# FIPS 203 Alg.15 Decrypt — 2-launch 编排技术总结

**读者**：未参与本仓库开发的 AscendC 实现者 / Agent  
**目的**：说明 Decrypt 全 device 链为何不能单 launch 融合 NTT+INTT，以及 **2 次 host launch** 的最小正确切分  
**案例锚点**：[`ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4`](../../ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/)（§附录）

---

## 1. 数学链（不变量）

Alg.15（ml_kem_1024 / k=4）设备侧顺序：

```text
u,v,ŝ ← unpack/decode(dk,c)
û     ← NTT(u)
ŵ     ← Σ_j MultiplyNTTs(ŝ[j], û[j])   // 单 poly 输出
w     ← INTT(ŵ_padded)
m     ← Compress₁ 逆 + 与 v 相减（extract 段）
```

Golden 仅验 I/O（`m.bin` 32B）；Host 禁止密码学计算，只搬 LUT 与 GM 缓冲。

---

## 2. MIX 段间同步原理

### 2.1 prep 与 NTT 不可同 kernel（SIM）

标量 prep（ByteDecode / Decompress）在 AIV 上完成；NTT 为 MIX（AIC MMAD + AIV split/pack/merge）。  
若 prep 与 NTT 处于**同一 launch 且 AIC 立即进入 NTT**，SIM 上常见 **`û` 对拍失败**（coeff 局部为 0），CPU tikicpu 仍可通过。

**模式 P-D1**：prep 独立 launch → `aclrtSynchronizeStream` → 再 launch NTT 链。

### 2.2 NTT 与 INTT 不可同 kernel（CrossCore flag）

三段式 NTT 与 INTT 均使用 CrossCore 状态 **1–3**（SPLIT / MMAD / PACK）。同一 launch 内 AIC 顺序执行「NTT → INTT」时，AIC 在 NTT MMAD 结束后即可进入 INTT，而 AIV 可能仍在 NTT S3 merge 写 `û`，或 INTT 与 NTT 争用同一 flag 导致 **`m` 错误**。

**模式 P-D2**：NTT 链与 INTT 链分为**两个 device kernel**，中间 host `aclrtSynchronizeStream`（可放在同一次 host launch 会话的第 2 次 aclrtLaunchKernel 内连续调用）。

### 2.3 无效手段（踩坑）

| 尝试 | 结果 |
|------|------|
| 单 launch `g4_full` 全无 sync | SIM `û` 错；~430k tick 能跑完 |
| CrossCore flag 4/0/10 段间握手 | SIM **死锁**（10min+）；flag 有效范围疑似仅 0–3 |
| PACK 二次握手（flag 3）在 CPU 未 guard | CPU tikicpu **假死** |
| 6 launch + 每段 sync | 正确；SIM ~405k tick，调度税高 |

---

## 3. 定稿 2-launch 编排

| Host launch | Device kernel | 内容 |
|-------------|---------------|------|
| **1** | `f203_decrypt_g4_prep` | unpack c → u,v；decode dk → ŝ（AIV，MIX 占位） |
| **2a** | `f203_decrypt_g4_chain_ntt` | NTT(u)→û；su_dot→ŵ；pad → wPadded |
| **2b** | `f203_decrypt_g4_chain_intt` | INTT(wPadded)→w_time；extract → m |

Launch 2 在 host 侧为**一次逻辑 launch**（prep 之后），内含 **2 个 kernel + 中间 sync**。

每个 kernel 后 **`aclrtSynchronizeStream`**（对齐 Encrypt G5）。

---

## 4. 验证

```bash
cd ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | 结果 |
|------|------|
| CPU | G1–G4 max=0 |
| SIM | G1–G4 max=0；Total tick **~427k** |

---

## 附录 A — 探针路径与文件

| 路径 | 说明 |
|------|------|
| `compute/g4_full/f203_decrypt_g4_prep_entry.cpp` | Launch-1 |
| `compute/g4_full/f203_decrypt_g4_chain_ntt_entry.cpp` | Launch-2a |
| `compute/g4_full/f203_decrypt_g4_chain_intt_entry.cpp` | Launch-2b |
| `main_decrypt_g4_run.cpp` | 单 ACL session 驱动 |
| `INTEGRATION_PLAN.md` / `STATUS.md` | 方案与验收 |

SEED_D=20260619；与 Encrypt round-trip 共用 `dk_pke` / `c` golden。
