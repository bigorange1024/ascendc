# TRACE.md — toy-e06-shake-ntt-basemul

对照知识库 §6（三位十进制）。本实验仅打印数字。  
**形态**：E05 壳 — L1=**真** SHAKE256；L2=**真**单 poly NTT + **真** basemul/MultiplyNTTs + 假 INTT TRACE + SET(4)。  
**语义**：NTT ≠ Tag5T；basemul=Alg.11/12（γ=kMlkemGammas）；SHAKE=hashlib.shake_256。

| 号 | 谁 | 含义 |
|----|----|------|
| 100 | Host | 将 launch L1 |
| 101 | Host | L1 Sync 返回 |
| 105 | Host | μ 空操作 |
| 110 | Host | 将 launch L2 |
| 111 | Host | L2 Sync 返回 |
| 200 | L1 AIV0 | 进入 L1 |
| 210 | L1 AIV0 | **真 SHAKE256** 开始 |
| 211 | L1 AIV0 | SHAKE 完成 |
| 212 | L1 AIV0 | UB golden PASS |
| 213 | L1 AIV0 | UB golden FAIL（不应出现） |
| 203 | L1 AIV0 | L1 将返回 |
| 400 | L2 AIC | L2 AIC 入口 |
| 401 | L2 AIC | 将 Wait(4) |
| 402 | L2 AIC | Wait(4) 成功 |
| 500 | L2 AIV0 | AIV0 入口 |
| 520 | L2 AIV0 | **真 NTT** 开始（Split） |
| 530 | L2 AIV0 | **真 basemul** 开始（非 stub） |
| 532 | L2 AIV0 | 真 basemul 完成 |
| 540 | L2 AIV0 | 假 INTT TRACE |
| 502 | L2 AIV0 | 已 SET(4) |
| 510 | L2 AIV1 | AIV1 入口 |
| 521 | L2 AIV1 | **真 NTT** 开始 |
| 531 | L2 AIV1 | **真 basemul** 开始 |
| 533 | L2 AIV1 | 真 basemul 完成 |
| 541 | L2 AIV1 | 假 INTT TRACE |
| 512 | L2 AIV1 | 已 SET(4) |

**NTT 内部 CrossCore**：flag 1/2；壳层 SET(4)=flag 4。  
**判读**：默认 3× Host `100/101/105/110/111`；每轮 L1 `200→210→211→212→203` 后 L2 真 NTT + 真 basemul `530→532`/`531→533` + `401/402` + `502/512`。
