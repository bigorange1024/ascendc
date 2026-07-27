# STATUS — exp-fips203-mlkem-pke-encrypt-k3

| 项 | 状态 |
|----|------|
| 阶段 | **有条件完成**（E14 incubating；CPU + `SIM_DIRECT=1` sim 通过；未跑 PKE roundtrip / NPU） |
| P | P3 |
| W | W4a |
| ID | E14 |
| CPU | **PASS** — `ENCRYPT_FORCE_REBUILD=1 bash run.sh -r cpu -v Ascend910B4`，`[cmp] c max=0` |
| SIM | **PASS** — `ENCRYPT_FORCE_REBUILD=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，Total tick **507633** |
| customspec | [`exp-fips203-mlkem-pke-encrypt-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-pke-encrypt-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.14 PKE Encrypt**（ML-KEM-768，`k=3`），对齐参数卡 §3.2 D14：

| 项 | 值 |
|----|----|
| I/O | `ek_pke.bin` **1184B** + `m.bin` 32B + `coins.bin` 32B → `c.bin` **1088B** |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute+tail `MIX blockDim=1` |
| prep | Â **[9,256] int32**，双 AIV **5+4**；`re` **[7,256]**（`r[3]‖e1[3]‖e2[1]`） |
| compute | NTT(`r`) k=3；INTT 真 batch4；`du/dv=10/4`；`c1=960B`、`c2=128B` |

## 验收记录（2026-07-26）

| 命令 | 结果 |
|------|------|
| `ENCRYPT_FORCE_REBUILD=1 bash run.sh -r cpu -v Ascend910B4` | **PASS**；`[cmp] c max=0`，`[SUCCESS] exp-fips203-mlkem-pke-encrypt-k3 (cpu)` |
| `ENCRYPT_FORCE_REBUILD=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；`[cmp] c max=0`，Total tick **507633**，Model RUN TIME **78349.9 ms** |
| 根目录 stray 检查 | **PASS**；用例根无 `core*.dump` / `profile_*_log*.toml` |

## 备注

- 源码从活跃 D14 k3 探针复制并自包含到本 exp；未从 `**/frozen/**` 复制。
- `multiply/` 仅 vendor 活跃 B6 k3 的 Alg.11 头文件，避免保留 sibling include。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
