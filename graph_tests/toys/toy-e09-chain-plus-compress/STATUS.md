# STATUS — toy-e09-chain-plus-compress

| 项 | 值 |
|----|----|
| task | E09 / `D-exp-e09` |
| 形态 | E08 2-launch 壳 + L1 **真** SHAKE256 + **真** CBD(η=2) + L2 **真** NTT + **真** basemul + **真** INTT + **真** Compress_d(d=4) + SET(4) |
| SoftSync | 默认不加 |
| SHAKE | 短向量 `abc`→32B；vendor 自包含 |
| CBD | 真 Alg.8 η=2 单 poly（`vendor/cbd_eta2/`） |
| NTT | ntt256 / `ntt_sim_kyber`（**≠ Tag5T**）；输入 = CBD(src) |
| basemul | 标量 Alg.11/12（`vendor/basemul_scalar/`） |
| INTT | E04/E07/E08 同系矩阵逆 Minv（**≠ Tag5T**） |
| **Compress** | **真** FIPS §4.2.1 d=4 向量 Barrett（`vendor/compress_d/` 自包含拷贝自 `pass-f203-compress-d-vec-k4`；双 AIV 各压 half） |
| 真算 | L1 SHAKE→CBD；L2 NTT→MultiplyNTTs→INTT→Compress |
| 默认轮次 | `TOY_ROUNDS=3` |
| 验收 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **PASS** |
| kernel wall | ≈ **97.0s** / budget 900；Total tick **695150** |
| Host TRACE | `100/101/105/110/111` ×3 |
| L1 TRACE | `200→210→211→212→220→221→203` ×3 |
| L2 TRACE | AIC `400/401/402`；AIV0 `500→520→530→532→540→542→550→552→502`；AIV1 对称 |
| golden | shake `diffs=0/32`；CBD `diffs=0/256`；Compress∘INTT∘basemul∘NTT(CBD) `diffs=0/256` |
| CrossCore | NTT flag 1/2；INTT flag 5/6；壳层 SET(4)=flag 4 |
| 积木 | SHAKE/Keccak + CBD η=2 + ntt256 + basemul_scalar + compress_d 自包含；未改原探针 / E01–E08 |
| AscendC API | 复用已登记 `TPipe`/`TBuf`/`TQue`/`DataCopy`/`Muls`/`Adds`/`ShiftRight`/`PipeBarrier`/`CrossCore*`/`printf`（Compress 路径同 probe 记录） |
| 根目录 stray dump | 无（stray 已收拢 `sim_log/`） |
| 未测 | SoftSync / OMIT_SET4 / Encrypt / Tag5T 整图 / retracted / d=5/10/11 |

**结论**：E08 壳上 INTT 后接入真 Compress_d(d=4)；上游 SHAKE→CBD→NTT→basemul→INTT+SET(4) 仍在；≥3 轮 SIM 不挂；Compress 短输出 golden 对拍通过。
