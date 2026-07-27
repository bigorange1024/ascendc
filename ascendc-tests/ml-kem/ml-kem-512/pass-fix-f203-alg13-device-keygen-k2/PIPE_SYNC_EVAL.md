# Pipe 同步评估 — pass-fix-f203-alg13-device-keygen-k2

## 当前结论

本探针是 ML-KEM-512 W2/D13 PKE KeyGen device k2：prep `AIV_ONLY blockDim=2`，compute `MIX blockDim=1`。本轮正确性优先，保留从活跃 k3 D13 与 512 W1 polyvec4 NTT 继承的显式 `PipeBarrier` / `CrossCore*` 同步点；CPU 与 `SIM_DIRECT=1` sim 均已 golden 对拍通过。

| 区段 | 同步口径 |
|------|----------|
| prep Â | 双 AIV 2+2，各自完成 SHAKE/Rej/NTT 后写 `a_hat[4,256]`；保留热路径 `PIPE_ALL`，不做性能删减 |
| prep s/e | `prf_out[4,192]` → CBD-η3 → `src[4,256]`；UB 阶段沿用已绿 B3b 的 MTE/Vector 屏障 |
| compute Stage1–3 | S-1 `MIX blockDim=1`；AIC/AIV 通过 GM + `CrossCore*` 交接；NTT S1–S3 禁 `Gather`，保持平面 `mat_c[32,128]` |
| encode | ByteEncode12 `2×384=768` 后融合 `ρ[32]`；输出只保留 `ek_pke.bin` 与 `dk_pke.bin` |

## 验收记录

| 命令 | 结果 |
|------|------|
| `KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4` | PASS；`ek_pke=800B`、`dk_pke=768B` golden PASS |
| `KEYGEN_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | PASS；`Total tick: 230102`；根无 stray dump |

## 后续可选

若后续要做性能删 barrier，必须以本文件的 k2 形状为准逐点重测 CPU + SIM；不得把 k3/k4 的 `5+4`、`8+8` 或 polyvec6/8 结论直接带入本目录。
