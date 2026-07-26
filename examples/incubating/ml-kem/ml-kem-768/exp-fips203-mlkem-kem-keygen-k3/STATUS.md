# STATUS — exp-fips203-mlkem-kem-keygen-k3

| 项 | 状态 |
|----|------|
| 阶段 | **有条件完成**（E19 incubating；CPU + `SIM_DIRECT=1` sim 通过；未跑 liboqs KAT / NPU） |
| 波次 | W4b / E19 |
| CPU | **PASS** — `bash run.sh -r cpu -v Ascend910B4`，`ek_kem`/`dk_kem` 对拍 max=0 |
| SIM | **PASS** — `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，Total tick **510867** |
| customspec | [`exp-fips203-mlkem-kem-keygen-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-kem-keygen-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.19 KEM KeyGen**（ML-KEM-768，`k=3`），对齐参数卡 §3.3 D19：

| 项 | 值 |
|----|----|
| I/O | `seed_d.bin` + LUT → `ek_kem.bin` **1184B** + `dk_kem.bin` **2400B** |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute+Alg.16 tail `MIX blockDim=1` |
| PKE 主体 | 复用 E13/D13 k3 几何：Â `[9,256]`、polyvec6、Inner 2+1、ByteEncode12 `3×384` |
| KEM 尾 | `dk_kem = dk_pke(1152)‖ek(1184)‖H(ek)(32)‖z(32)`，内嵌第二 launch，无第三 launch |

## 验收记录（2026-07-26）

| 命令 | 结果 |
|------|------|
| `bash run.sh -r cpu -v Ascend910B4` | **PASS**；`[verify] PASS ek_kem.bin max=0 (1184 bytes)`；`[verify] PASS dk_kem.bin max=0 (2400 bytes)` |
| `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；`Total tick: 510867`，`Model RUN TIME: 79193 ms` |
| 根目录 stray 检查 | **PASS**；用例根无 `core*.dump` / `profile_*_log*.toml` / `*.dump` |

## 备注

- 源码从活跃 D19/E13/D13 k3 路径复制并自包含到本 exp；未从 `**/frozen/**` 复制。
- `scripts/compute`、`thirdparty`、`scripts/keygen_golden.py` 均为本目录实体文件；`run.sh` 不创建运行时探针软链。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
