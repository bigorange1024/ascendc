# STATUS — exp-fips203-mlkem-kem-encaps-k3

| 项 | 状态 |
|----|------|
| 阶段 | **有条件完成**（E20 incubating；CPU + `SIM_DIRECT=1` sim 通过；未跑 liboqs KAT / NPU） |
| 波次 | W4b / E20 |
| CPU | **PASS** — `bash run.sh -r cpu -v Ascend910B4`，`c`/`K` 对拍 max=0 |
| SIM | **PASS** — `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，Total tick **590261** |
| customspec | [`exp-fips203-mlkem-kem-encaps-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-kem-encaps-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.20 KEM Encaps**（ML-KEM-768，`k=3`），对齐参数卡 §3.3 D20：

| 项 | 值 |
|----|----|
| I/O | `ek_kem.bin` **1184B** + `m.bin` 32B + LUT → `c.bin` **1088B** + `K.bin` 32B |
| Launch | SIM **2**：KEM head+prep `AIV_ONLY blockDim=2` → compute+pack `MIX blockDim=1`；CPU 可多分段 |
| KEM 头 | `H(ek)` 与 `G(m‖H(ek))` 在设备侧生成 `K‖r`，`r` 供 Encrypt prep 消费 |
| PKE 主体 | 复用 E14/D20 k3 几何：Â `[9,256]`、`re[7,256]`、INTT batch4、`du/dv=10/4` |

## 验收记录（2026-07-26）

| 命令 | 结果 |
|------|------|
| `bash run.sh -r cpu -v Ascend910B4` | **PASS**；`[verify] c.bin max_abs_diff=0`；`[verify] K.bin max_abs_diff=0` |
| `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；`Total tick: 590261`，`Model RUN TIME: 98639.5 ms` |
| 根目录 stray 检查 | **PASS**；用例根无 `core*.dump` / `profile_*_log*.toml` / `OPPROF_*` |

## 备注

- 源码从活跃 D20/E14/D14 k3 路径复制并自包含到本 exp；未从 `**/frozen/**` 复制。
- `compute/`、`prep/`、`multiply/`、`scripts/host_golden/` 均为本目录实体文件；`run.sh` 不依赖其它用例路径。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
