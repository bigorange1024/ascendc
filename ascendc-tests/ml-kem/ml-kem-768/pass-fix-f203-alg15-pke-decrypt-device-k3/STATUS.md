# STATUS — pass-fix-f203-alg15-pke-decrypt-device-k3

FIPS 203 **Alg.15 K-PKE.Decrypt**（ML-KEM-768 / k=3）**device 探针 PASS**。

来源：从活跃 `ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/` 复制，按 ML-KEM-768 参数卡 §3.2 retarget；未使用 `frozen/`，未写 `examples/incubating` / `examples/stable`。

| 项 | 值 |
|----|-----|
| **阶段** | **PASS**（D15；W2 PKE 三段闭环完成） |
| **I/O** | `dk_pke=1152B` + `c=1088B` + `lut_*` → **仅** `m=32B` |
| **密文布局** | `c1=3×320B`（`d_u=10`），`c2=128B`（`d_v=4`） |
| **SEED_D** | 20260619 |
| **Launch** | **1×** `f203_decrypt_device_fused`；host `blockDim=1` |
| **prep** | ByteDecode₁₀/₄/₁₂ 标量 + Decompress₁₀/₄ 向量 |
| **compute** | NTT(u') k=3（AIV 2+1）→ `ŝ·û` → INTT(ŵ padded) → Compress₁+ByteEncode₁ |
| **SIM tick** | **222032**（Cloud / `SIM_DIRECT=1` / Ascend910B4） |

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**；`[verify] PASS max=0 (32 bytes)`；`m.bin=32B` |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；`[verify] PASS max=0 (32 bytes)`；Total tick **222032**；Model RUN TIME **34228.8 ms** |

根目录检查：`core*.dump` / `profile_*_log*.toml` **none**；CaModel stray 由脚本收拢到 `sim_log/`。

## 单 kernel 要点

| 项 | 说明 |
|----|------|
| FSM | NTT flag 1/2/3；INTT flag 1/3；段间 GATE 4→8 |
| softSyncGm | prep / su_dot 仅 AIV0；AIV1 哨兵汇合后再双 AIV Set(4) |
| k=3 分片 | NTT / INTT Stage1/3 使用 AIV0=poly0/1、AIV1=poly2；不读写第 4 个假 poly |
| su_dot→INTT | `ŵ` pad 到 k=3 polyvec 前缀后复用 Stage123；尾段只取第 0 个 poly 的 INTT 输出 |
| 尾段融合 | `w=(v'-w_time) mod q` 向量 → Compress₁ 原地 → ByteEncode₁ 标量 pack |

## 遗留

- 暂未接 D14 roundtrip 脚本；本轮完成最小验收：独立 `dk_pke+c→m` CPU + SIM 均对拍 golden。
- W3 D19/D20/D21 仍为后续 KEM device 探针。
