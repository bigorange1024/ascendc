# STATUS — fix-f203-alg14-pke-encrypt-correctness-k4

**定位**：Alg.14 **设备 AscendC 拼装**正确性探针（见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)、[`SELF_CONTAINED.md`](SELF_CONTAINED.md)）。

**阶段**：**G1–G3 CPU+SIM ✅**；**G4/G5 CPU 全链 ✅**；**G4/G5 SIM `c.bin` 阻塞**（见 §G5）。

**审计**：[`G3_SIM_AUDIT.md`](G3_SIM_AUDIT.md) — 507000、双 session at_r、G5 时间线 **§9–§10**。

## 验收命令

```bash
# 中间张量（G1–G3，默认 run.sh 末尾 verify_gate）
ENCRYPT_GATE=3 bash run.sh -r cpu -v Ascend910B4
ENCRYPT_KERNEL_BUDGET_SEC=600 ENCRYPT_GATE=3 bash run.sh -r sim -v Ascend910B4

# 全链 c.bin（须 ENCRYPT_VERIFY=1）
ENCRYPT_VERIFY=1 ENCRYPT_GATE=5 bash run.sh -r cpu -v Ascend910B4
ENCRYPT_VERIFY=1 ENCRYPT_KERNEL_BUDGET_SEC=900 ENCRYPT_GATE=5 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 结果（2026-06-29 晚，复验口径分离）

| Gate | 对拍脚本 | CPU | SIM | 备注 |
|------|----------|-----|-----|------|
| G0 | — | PASS | — | marker 壳 |
| G1 | `verify_gate` | PASS | PASS | max=0 |
| G2 | `verify_gate` | PASS | PASS | NTT r̂ |
| G3 | `verify_gate` | PASS | PASS | CPU `g3_linear`；SIM 双 session `at_r` |
| G4 | `verify_gate` + `verify_result` | gate ✅ / **c ✅** | gate ✅ / **c ❌** | SIM c.bin @382：`c=0 g=255` |
| G5 | 同上 | gate ✅ / **c ✅** | gate ✅ / **c ❌** | 默认 `ENCRYPT_GATE=5` |

| 模式 | `verify_gate` G1–G3 | `ENCRYPT_VERIFY=1` c.bin | wall（G5） |
|------|---------------------|----------------------------|------------|
| CPU G5 | max=0 | **max=0** 1568B | ~10s |
| SIM G5 | max=0 | **FAIL** | ~360s；日志 507000 + 末尾 `free(): invalid pointer` |

**汇报可用**：`ENCRYPT_VERIFY=1 ENCRYPT_GATE=5 bash run.sh -r cpu -v Ascend910B4`（设备 decode + 全链，无 `t_hat.bin` staging）。

## G4 实现要点

- **INTT**：`compute/intt/` — stage123 k=4
- **噪声/μ**：`compute/g4/f203_encrypt_g4_noise`
- **Pack**：`pack/f203_encrypt_pack` — Compress₁₁/₅ + ByteEncode → c₁‖c₂
- **golden**：`scripts/host_golden/golden_c.py`；`gen_ek_pke.py`

## G5（进行中 — SIM c.bin 阻塞）

**目标**：整条 Encrypt 在 device（无 Host `t_hat.bin` / fake-Â / `pack_t_hat` 算法绕行）。

**已实现**：

- `prep/decode_ek/f203_encrypt_decode_t_hat` — 设备 ByteDecode₁₂ ek→t̂ + 写 Â 列 0
- CPU：单 session 全链（`main_encrypt_g5_run.cpp` → `g3_linear4`）
- SIM phase1：prep→NTT→decode→at_r→û + D2H aCol0；phase2：`run_g3_at_r_device_once`（G4 独立 session）→ tr̂；phase3：复用 G4 INTT/noise/pack

**阻塞（SIM 全链）**：

| # | 现象 | 待查 |
|---|------|------|
| B1 | `ENCRYPT_VERIFY=1` c.bin max=255 @382（输出 0 vs golden 255） | G4 tail INTT/noise/pack 在 SIM 多 session 后 |
| B2 | 日志 `LaunchAscendKernel ret 507000` | 非 G3 张量错（gate 仍 max=0） |
| B3 | 末尾 `free(): invalid pointer` / signal 6 | ACL 生命周期或多段 Finalize |

**下一 Agent**：先修 SIM `c.bin`（G4/G5 共用 tail）；勿回退 Host decode/`t_hat.bin`。
