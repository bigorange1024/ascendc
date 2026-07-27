# STATUS — pass-fix-f203-alg19-kem-keygen-device-k2

**状态**：**有条件完成**（2026-07-27，CPU + `SIM_DIRECT=1` sim golden 对拍通过；未跑 liboqs KAT / NPU）。

## 语义与锁定参数

FIPS 203 **Alg.19 ML-KEM KeyGen**（ML-KEM-512，`k=2`），对齐参数卡 §3.3：

| 项 | 值 |
|----|----|
| 生产 I/O | `input/seed_d.bin` + `lut_even/odd_stacked.bin` → `output/ek_kem.bin` **800B** + `dk_kem.bin` **1632B** |
| dk 布局 | `dk_pke(768)‖ek(800)‖H(ek)(32)‖z(32)` |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute `MIX blockDim=1` + Alg.16 尾 |
| prep | 复用 D13 k2：Â **[4,256] int32**，双 AIV **2+2**；`s‖e` **[4,256]**，η1=3 |
| compute | 复用 D13 k2：polyvec4 NTT；InnerProduct `P_OUT=S_VEC=2`，AIV **1+1**；禁零垫 |
| Derand | `d`: `exp-mlkem-f203-2s1e-k2:SEED_D=`；`z`: `exp-mlkem-f203-kem-k2:SEED_Z=` |

## 验收记录

| 命令 | 结果 | 证据 |
|------|------|------|
| `bash run.sh -r cpu -v Ascend910B4` | **PASS** | `ek_kem.bin max=0 (800 bytes)`；`dk_kem.bin max=0 (1632 bytes)` |
| `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS** | `Total tick: 320247`；`ek_kem`/`dk_kem` max=0；用例根无 stray dump |

## 实现要点

- 从活跃 D13 k2 探针复制 prep/compute 几何，只追加 D19 KEM 尾段；未使用 `**/frozen/**` 源码。
- `ek_kem` 与 D13 `ek_pke` 共用输出缓冲；AIV0 在 `FuseEkPke` 后计算 `H(ek)`、派生 `z` 并拼接 `dk_kem`。
- Host golden 复用 D13 k2 oracle，仅作为 I/O oracle 生成 `ek_kem` / `dk_kem`，不作为 AscendC 实现规格。
