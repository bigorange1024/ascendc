# STATUS — exp-fips203-mlkem-pke-decrypt-k3

| 项 | 状态 |
|----|------|
| 阶段 | **有条件完成**（E15 incubating；CPU + `SIM_DIRECT=1` sim 通过；未跑 PKE roundtrip / NPU） |
| 波次 | W4a / E15 |
| CPU | **PASS** — `bash run.sh -r cpu -v Ascend910B4`，`[verify] PASS max=0 (32 bytes)` |
| SIM | **PASS** — `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，Total tick **222073** |
| customspec | [`exp-fips203-mlkem-pke-decrypt-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-pke-decrypt-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.15 PKE Decrypt**（ML-KEM-768，`k=3`），对齐参数卡 §3.2 D15：

| 项 | 值 |
|----|----|
| I/O | `dk_pke.bin` **1152B** + `c.bin` **1088B** → `m.bin` **32B** |
| Launch | **1**：fused `MIX blockDim=1` |
| prep | ByteDecode₁₀/₄/₁₂ 标量；Decompress₁₀/₄ 向量 |
| compute | NTT/INTT k=3；AIV **2+1**；tail Compress₁ + ByteEncode₁ |

## 验收记录（2026-07-26）

| 命令 | 结果 |
|------|------|
| `bash run.sh -r cpu -v Ascend910B4` | **PASS**；`[verify] PASS max=0 (32 bytes)`，`[SUCCESS] exp-fips203-mlkem-pke-decrypt-k3 (cpu)` |
| `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；`[verify] PASS max=0 (32 bytes)`，Total tick **222073**，Model RUN TIME **33721.3 ms** |
| 根目录 stray 检查 | **PASS**；用例根无 `core*.dump` / `profile_*_log*.toml` |

## 备注

- 源码从活跃 D15 k3 探针复制并自包含到本 exp；未从 `**/frozen/**` 复制。
- `scripts/vendor_sync.sh` 仅做本目录文件存在性检查，不会从 k4/frozen 覆盖本目录。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
