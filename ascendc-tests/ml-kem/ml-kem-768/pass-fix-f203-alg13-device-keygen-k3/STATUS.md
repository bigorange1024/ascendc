# STATUS — pass-fix-f203-alg13-device-keygen-k3

**状态**：**有条件完成**（2026-07-26，CPU + `SIM_DIRECT=1` sim golden 对拍通过；未跑 liboqs KAT / NPU）。

## 语义与锁定参数

FIPS 203 **Alg.13 PKE KeyGen**（ML-KEM-768，`k=3`），对齐参数卡 §3.2：

| 项 | 值 |
|----|----|
| 生产 I/O | `input/seed_d.bin` + `lut_even/odd_stacked.bin` → `output/ek_pke.bin` **1184B** + `dk_pke.bin` **1152B** |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute `MIX blockDim=1` |
| prep | Â **[9,256] int32**，双 AIV **5+4**；`s‖e` **[6,256]**，η1=η2=2 |
| compute | polyvec6 NTT：S0 **[12,256]**，mat_c **[48,128]**；InnerProduct `P_OUT=S_VEC=3`，AIV **2+1** |
| encode | ByteEncode12 **3×384=1152**，再融合 `ek‖ρ` → **1184** |
| Derand | `exp-mlkem-f203-2s1e-k3:SEED_D=`；`G(d‖byte(k))` 使用 `k=3` |

## 验收记录

| 命令 | 结果 | 证据 |
|------|------|------|
| `KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4` | **PASS** | `ek_pke.bin PASS (bytes=1184)`；`dk_pke.bin PASS (bytes=1152)` |
| `KEYGEN_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS** | `Total tick: 373426`；`Model RUN TIME: 64139.8 ms`；ek/dk golden PASS |
| 根目录 stray 检查 | **PASS** | 用例根无 `core*.dump` / `profile_*_log*.toml` / `OPPROF_*` |

## 实现要点

- 从活跃 k4 KeyGen 探针复制后改为 k3；未使用 `**/frozen/**` 源码。
- prep 的 `aHatQue` 复用 PRF 队列，但按 `max(PRF_Y[6,128], â[256] int32)` 分配，避免 5+4 分片下单 poly 输出 UB 不足。
- `scripts/keygen_golden.py` / `scripts/compute/gen_data.py` 已改为真实 k3 oracle：`Â[9]`、`src[6]`、`S0[12]`、`mat_c[48]`、`dst[6]`、`ek/dk=1184/1152`。

## 未做

- liboqs KAT 已 retarget 到 `ml_kem_768` 胶水，但本轮未跑。
- NPU 实机未跑。
