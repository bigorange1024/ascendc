# STATUS — exp-fips203-mlkem-pke-keygen-k3

| 项 | 状态 |
|----|------|
| 阶段 | **有条件完成**（E13 incubating；CPU + `SIM_DIRECT=1` sim 通过；未跑 liboqs KAT / NPU） |
| 波次 | W4a / E13 |
| CPU | **PASS** — `bash run.sh -r cpu -v Ascend910B4`，输出 `ek_pke=1184B` / `dk_pke=1152B` |
| SIM | **PASS** — `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，Total tick **373429** |
| golden | **PASS** — `KEYGEN_GOLDEN_ONLY=1 python3 scripts/gen_data.py && python3 scripts/verify_production.py`，ek/dk 对拍通过 |
| customspec | [`exp-fips203-mlkem-pke-keygen-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-pke-keygen-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.13 PKE KeyGen**（ML-KEM-768，`k=3`），对齐参数卡 §3.2 D13：

| 项 | 值 |
|----|----|
| I/O | `seed_d.bin` + LUT → `ek_pke.bin` **1184B** + `dk_pke.bin` **1152B** |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute `MIX blockDim=1` |
| prep | Â **[9,256] int32**，双 AIV **5+4**；`s‖e` **[6,256]** |
| compute | polyvec6 NTT；S0 **[12,256]**，mat_c **[48,128]**；InnerProduct `P=3`，AIV **2+1** |

## 验收记录（2026-07-26）

| 命令 | 结果 |
|------|------|
| `bash run.sh -r cpu -v Ascend910B4` | **PASS**；`[keygen] output OK ek_pke=1184B dk_pke=1152B` |
| `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；`Total tick: 373429`，`Model RUN TIME: 61338 ms` |
| `KEYGEN_GOLDEN_ONLY=1 python3 scripts/gen_data.py && python3 scripts/verify_production.py` | **PASS**；`ek_pke.bin PASS (bytes=1184)`；`dk_pke.bin PASS (bytes=1152)` |
| 根目录 stray 检查 | **PASS**；用例根无 `core*.dump` / `profile_*_log*.toml` |

## 备注

- 源码从活跃 D13 k3 探针复制并自包含到本 exp；未从 `**/frozen/**` 复制。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
