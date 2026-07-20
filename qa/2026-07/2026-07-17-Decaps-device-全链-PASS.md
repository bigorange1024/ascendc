# 2026-07-17 — Decaps device Phase-E · Phase-D · 全链 PASS

## 1. 锁定（先 E 后 D）

[`pass-fix-f203-alg21-kem-decaps-device-k4/INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/INTEGRATION_PLAN.md) 写死：先 E 后 D；G 融 Encrypt prep；设备 FO；SIM 允许 2-session。

## 2. Phase-E（午前）

Gate **E0–E2 PASS**：CPU+SIM `K` max=0；tick **746221**。G 融 prep；CPU `pack_fo`；SIM `fo_only` 过渡。

## 3. Phase-D + 全链（午后）

| 项 | 说明 |
|----|------|
| Phase-D | stable `f203_decrypt_device_fused`；`dk_kem` 前缀作 dk_pke |
| 全链 Host | `main_kem_decaps.cpp`：D→E；SIM `decaps_2session` |
| gen_data | `scripts/gen_data.py`：dk_kem+c+双套 LUT+golden K |
| CMake | CPU **单库**；SIM **双库**（Decrypt/Encrypt 同名头，precompile 无法 per-TU 隔离）+ 强制 2-session |

| 模式 | 结果 |
|------|------|
| CPU 全链 | `K` max=0 |
| SIM 全链 | `K` max=0；D tick **283317** + E tick **745341** |

Gate：**D / F1 PASS**。

## 5. Gate E3 拒绝路径（同日补）

`KEM_DECAPS_REJECT=1`：`os.urandom(1568)` 假密文 → device Decaps 与 **liboqs Decaps** 对拍 `K`（≡ `J(z‖c)`）。CPU+SIM **REJECT PASS**。

注：liboqs 不暴露内部重加密 `c'`；行 9–11 对外可观测输出是 `K`。

## 6. scripts 默认改指 device（同日补）

已把仓库级 Decaps 默认从 correctness 切到 [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)：

| 脚本 | 调整 |
|------|------|
| `scripts/roundtrip_kem_decaps.sh` | 默认 `DECAPS_DIR`→device；合法路径传 `M_FILE` |
| `scripts/roundtrip_kem_keygen_encaps_decaps.sh` | 默认 `DECAPS_DIR`→device；stash `m.bin`；拒绝路径改 `c_bad + KEM_DECAPS_REJECT=1` |
| `scripts/liboqs_kem_vs_ascendc.sh` | 默认 `DECAPS_DIR`→device；合法路径传 fixture `m`/`K`；拒绝路径用 `c_bad` |
| `scripts/kat_liboqs_kem_decaps.py` | 默认 `DECAPS_DIR`→device；每轮传 `M_HEX` |

验证：`KEM_DEC_CPU_TRIALS=1 KEM_DEC_SIM_TRIALS=0 python3 scripts/kat_liboqs_kem_decaps.py` PASS。

## 7. T2 交 Cloud Agent（同日定）

用户确认 **SIM 单库 / 单 session 真修仍有必要**，但本机不改码；**交 Cloud Agent** 做 T2。

| 项 | 说明 |
|----|------|
| **改什么** | 工程折中（CMake 合库、头/符号隔离、host session 编排）；**不**改 Alg.18 |
| **现状保底** | SIM 双库 + `decaps_2session`（全链已绿） |
| **目标** | 单 `libascendc_kernels_*.so` + 同 session 连续 D→E（尽量默认 1-session 绿） |
| **交接面** | [`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md) ★下一刀；[`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/INTEGRATION_PLAN.md) |

本机本轮：刷新文档并 **git push** 基线，供 Cloud 拉仓开工。

## 8. T2 PASS（同日 Cloud 接替）

Cloud Agent 落地 SIM **单库 + 默认 1-session**：

| 项 | 说明 |
|----|------|
| **shim** | `scripts/prepare_dec_shim.sh` ← stable Decrypt；冲突头 → `dec_*`（`shim/` gitignore） |
| **CMake** | SIM/NPU 单 `ascendc_library`；CPU 仍 per-source `-I` |
| **默认** | `ASCENDC_SIM_HOST_MODE=decaps_1session`；`decaps_2session` 对照 |
| **验收** | CPU/SIM 全链 `K` max=0；E3 REJECT CPU+SIM PASS；仅 `libascendc_kernels_sim.so` |
| **tick** | D**286803** + E**745925** |

Gate：**T2 PASS**。下一：`pass-fix` 更名 / KAT 扩量 / `#交付#`。
