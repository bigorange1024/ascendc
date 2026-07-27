# STATUS — pass-fix-f203-alg13-device-keygen-k2

**状态**：**有条件完成**（2026-07-27，CPU + `SIM_DIRECT=1` sim golden 对拍通过；未跑 liboqs KAT / NPU）。

## 语义与锁定参数

FIPS 203 **Alg.13 PKE KeyGen**（ML-KEM-512，`k=2`），对齐参数卡 §3.2：

| 项 | 值 |
|----|----|
| 生产 I/O | `input/seed_d.bin` + `lut_even/odd_stacked.bin` → `output/ek_pke.bin` **800B** + `dk_pke.bin` **768B** |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute `MIX blockDim=1` |
| prep | Â **[4,256] int32**，双 AIV **2+2**；`s‖e` **[4,256]**，η1=η2=3 |
| compute | polyvec4 NTT：S0 **[8,256]**，mat_c **[32,128]**；InnerProduct `P_OUT=S_VEC=2`，AIV **1+1** |
| encode | ByteEncode12 **2×384=768**，再融合 `ek‖ρ` → **800** |
| Derand | `exp-mlkem-f203-2s1e-k2:SEED_D=`；`G(d‖byte(k))` 使用 `k=2` |

## 验收记录

| 命令 | 结果 | 证据 |
|------|------|------|
| `KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4` | **PASS** | `ek_pke.bin PASS (bytes=800)`；`dk_pke.bin PASS (bytes=768)` |
| `KEYGEN_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS** | `Total tick: 230102`；`Model RUN TIME: 49726.9 ms`；ek/dk golden PASS |
| 根目录 stray 检查 | **PASS** | 用例根无 `core*.dump` / `profile_*_log*.toml` / `OPPROF_*` |

## 实现要点

- 从活跃 k3 KeyGen 探针复制后改为 k2；未使用 `**/frozen/**` 源码。
- prep 的 `aHatQue` 复用 PRF 队列，但按 `max(PRF_Y[4,192], â[256] int32)` 分配，避免单 poly 输出 UB 不足。
- `scripts/keygen_golden.py` / `scripts/compute/gen_data.py` 已改为真实 k2 oracle：`Â[4]`、`src[4]`、`S0[8]`、`mat_c[32]`、`dst[4]`、`ek/dk=800/768`。

## 未做

- liboqs KAT 已 retarget 到 `ml_kem_512` 胶水，但本轮未跑。
- NPU 实机未跑。
