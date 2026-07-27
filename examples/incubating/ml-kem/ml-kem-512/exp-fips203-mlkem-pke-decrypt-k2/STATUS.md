# STATUS — exp-fips203-mlkem-pke-decrypt-k2

FIPS 203 **Alg.15 K-PKE.Decrypt**（ML-KEM-512 / k=2）**device 探针**。

来源：从活跃 `ml-kem-768/pass-fix-f203-alg15-pke-decrypt-device-k3/` 复制，按 ML-KEM-512 参数卡 §3.2 retarget；未使用 `frozen/`，未写 `examples/incubating` / `examples/stable`。

| 项 | 值 |
|----|-----|
| **阶段** | **D15 PASS**（独立 `dk_pke+c→m`；当前分支未见 D14，未接 D14↔D15 roundtrip） |
| **I/O** | `dk_pke=768B` + `c=768B` + `lut_*` → **仅** `m=32B` |
| **密文布局** | `c1=2×320B`（`d_u=10`），`c2=128B`（`d_v=4`） |
| **SEED_D** | 20260619 |
| **Launch** | **1×** `f203_decrypt_device_fused`；host `blockDim=1` |
| **prep** | ByteDecode₁₀/₄/₁₂ 标量 + Decompress₁₀/₄ 向量 |
| **compute** | NTT(u') k=2（AIV 1+1）→ `ŝ·û` → INTT(ŵ padded) → Compress₁+ByteEncode₁ |
| **SIM tick** | **168975**（Cloud / `SIM_DIRECT=1` / Ascend910B4） |

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**；`[verify] PASS max=0 (32 bytes)`；`m.bin=32B` |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；`[verify] PASS max=0 (32 bytes)`；Total tick **168975**；Model RUN TIME **20906.4 ms** |

根目录检查：`core*.dump` / `profile_*_log*.toml` **none**；CaModel stray 由脚本收拢到 `sim_log/`。

## 单 kernel 要点

| 项 | 说明 |
|----|------|
| FSM | NTT flag 1/2/3；INTT flag 1/3；段间 GATE 4→8 |
| softSyncGm | prep / su_dot 仅 AIV0；AIV1 哨兵汇合后再双 AIV Set(4) |
| k=2 分片 | NTT / INTT Stage1/3 使用 AIV0=poly0、AIV1=poly1；不读写假 poly |
| su_dot→INTT | `ŵ` pad 到 k=2 polyvec 前缀后复用 Stage123；尾段只取第 0 个 poly 的 INTT 输出 |
| 尾段融合 | `w=(v'-w_time) mod q` 向量 → Compress₁ 原地 → ByteEncode₁ 标量 pack |

## 遗留

- 当前分支未见 `pass-fix-f203-alg14-pke-encrypt-device-k2/`；本轮按用户要求完成 D15 自包含 golden 验收。
- W3 D19/D20/D21 仍为后续 KEM device 探针。
