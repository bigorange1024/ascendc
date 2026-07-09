# STATUS — pass-fix-f203-alg15-pke-decrypt-device-k4

FIPS 203 **Alg.15 K-PKE.Decrypt**（ml_kem_1024 / k=4）**优化实现探针（终态 PASS）**。

自 `fix-f203-alg15-pke-decrypt-device-k4` 改名（2026-07-09）：CPU+SIM + roundtrip 已验收；**已晋级** [`stable-fips203-mlkem-pke-decrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/)（2026-07-10，T15a）。

| 项 | 值 |
|----|-----|
| **阶段** | **PASS**（单 kernel + 尾段融合；UB 驻留两档实验均已回滚） |
| **I/O** | **生产** `input/`：`dk_pke`+`c`+`lut_*` → **仅** `output/m.bin`（**禁止**中间态 D2H；夹具在 `output/_gen_fixture/`） |
| **注释** | 主路径与 incubating exp 同步补详细中文注释（2026-07-09） |
| **SEED_D** | 20260619 |
| **Launch** | **1×** `f203_decrypt_device_fused`（GATE 4/8 + softSync AIV0/1） |
| **prep** | ByteDecode₁₁/₅/₁₂ **标量**（落盘 DataCopy）+ Decompress₁₁/₅ **向量** |
| **尾** | v−w 向量 → Compress₁ **原地** → Encode₁ 按字节拼（**无独立 bits[256]**） |
| **SIM tick** | **283252**（生产：尾融合 + `pad→wPadded` + `Process()`；回滚后复测） |

## 验收

| 模式 | 结果 |
|------|------|
| CPU | `m` max=0 |
| SIM | `m` max=0；tick **283252**；根目录无 stray dump |

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## 单 kernel 要点

| 项 | 说明 |
|----|------|
| FSM | NTT flag 1/2/3；INTT flag 1/3；段间 GATE 4→8 |
| softSyncGm | prep / su_dot 仅 AIV0；AIV1 哨兵汇合后再双 AIV Set(4) |
| GM 可见性 | `decode_s_hat` / `pad_w_hat` **禁止**标量写 GM；须 UB + `DataCopy`/`Duplicate` |
| 尾段融合 | Compress 写回 `wCan`；Encode 每 8 lane→1B |
| su_dot→INTT | **生产**：`pad→wPadded` + `Process()` |

## UB 驻留实验（两档均已回滚，2026-07-09）

| 档 | 做法 | SIM tick | vs 基线 ~283278 |
|----|------|----------|-----------------|
| **A 半吊子** | `su_dot→wHatGm` → 另 pipe pad UB → `ProcessFromLocal` | **287680 / 287687 / 287700** | **+~1.5%** |
| **B 同 TPipe** | `KernelSuDot::ProcessToPadUb`（ŵ 在同 pipe `padBuf_`）→ `ProcessFromLocal`；不写 `wPadded` | **287463 / 287411** | **仍 +~1.5%** |
| CPU | 两档 `ProcessFromLocal` 均对拍失败 → 须分叉 `Process()` | — | — |

| 结论 | 说明 |
|------|------|
| **判决** | **两档均不采纳**；生产回滚 `pad→wPadded` + `Process()` |
| **为何同 TPipe 仍无收益** | 省掉 `wPadded` 全量写盘后，仍有 pad `Duplicate` + Stage1 自有 TPipe；相对基线向量 `pad`+`Process()` 的 GM 路径，净 tick 不降 |
| **再开条件** | 除非能把 Stage1 split **并入** su_dot 同一 `TPipe` 且去掉 pad 零填充开销，否则不再试本段 UB 驻留 |

## 跨探针正确性（仓库脚本默认本目录）

| 脚本 | 说明 |
|------|------|
| [`scripts/roundtrip_pke_encrypt_decrypt.sh`](../../scripts/roundtrip_pke_encrypt_decrypt.sh) | **stable KeyGen + stable Encrypt** → **本 Decrypt**；验 `m` vs 明文；**CPU+SIM PASS**（2026-07-09，`SEED_D=20260619`；SIM tick **283248**） |
| [`scripts/roundtrip_pke_batch.sh`](../../scripts/roundtrip_pke_batch.sh) | 默认 CPU×10 + SIM×1 |
| [`scripts/liboqs_pke_vs_ascendc.sh`](../../scripts/liboqs_pke_vs_ascendc.sh) | Decrypt 段默认本目录 vs liboqs |

回退 2-launch：`DECRYPT_DIR=.../fix-f203-alg15-pke-decrypt-correctness-k4`。

## 下一步

- 晋级 `examples/stable/stable-fips203-mlkem-pke-decrypt-k4`（**T15a 已完成**，2026-07-10）
- Decode 向量化（低 ROI，非阻塞）
