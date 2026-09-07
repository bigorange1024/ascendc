# TRACE.md — toy-e07-shake-ntt-basemul-intt

**形态**：E06 壳 — L1=**真** SHAKE256；L2=**真**单 poly NTT + **真** basemul + **真** INTT + SET(4)。  
**语义**：NTT/INTT ≠ Tag5T（= ntt256 矩阵正/逆）；basemul=Alg.11/12；SHAKE=hashlib.shake_256。

## Host

| code | 含义 |
|------|------|
| 100 | 轮次开始 / 将 launch L1 |
| 101 | L1 Sync 完成 |
| 105 | Host μ 空操作 |
| 110 | 将 launch L2 |
| 111 | L2 Sync 完成 |

## L1（真 SHAKE）

| code | 核 | 含义 |
|------|----|------|
| 200 | AIV0 | 进入 L1 |
| 210 | AIV0 | 真 SHAKE256 开始 |
| 211 | AIV0 | SHAKE 完成 |
| 212 | AIV0 | UB golden PASS |
| 213 | AIV0 | UB golden FAIL（不应出现） |
| 203 | AIV0 | L1 将返回 |

## L2

| code | 核 | 含义 |
|------|----|------|
| 400 | AIC | L2 AIC 入口 |
| 401 | AIC | 将 Wait(4) |
| 402 | AIC | Wait(4) 成功 |
| 500 | L2 AIV0 | 入口 |
| 520 | L2 AIV0 | 真 NTT 开始 |
| 530 | L2 AIV0 | 真 basemul 开始 |
| 532 | L2 AIV0 | 真 basemul 完成 |
| 540 | L2 AIV0 | **真 INTT** 开始（非 stub） |
| 542 | L2 AIV0 | 真 INTT 完成 |
| 502 | L2 AIV0 | SET(4) 后出口 |
| 510 | L2 AIV1 | 入口 |
| 521 | L2 AIV1 | 真 NTT 开始 |
| 531 | L2 AIV1 | 真 basemul 开始 |
| 533 | L2 AIV1 | 真 basemul 完成 |
| 541 | L2 AIV1 | **真 INTT** 开始 |
| 543 | L2 AIV1 | 真 INTT 完成 |
| 512 | L2 AIV1 | SET(4) 后出口 |

**判读**：默认 3× Host `100/101/105/110/111`；每轮 L1 `200→210→211→212→203` 后 L2 真 NTT + 真 basemul `530→532`/`531→533` + 真 INTT `540→542`/`541→543` + `401/402` + `502/512`。

**CrossCore**：NTT flag 1/2；INTT flag 5/6；壳层 SET(4)=flag 4。
