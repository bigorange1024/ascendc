# STATUS — toy-e08-shake-cbd-ntt-chain

| 项 | 值 |
|----|----|
| task | E08 / `D-exp-e08` |
| 形态 | E07 2-launch 壳 + L1 **真** SHAKE256 + **真** CBD(η=2) + L2 **真** NTT + **真** basemul + **真** INTT + SET(4) |
| SoftSync | 默认不加 |
| SHAKE | 短向量 `abc`→32B；vendor 自包含 |
| **CBD** | **真** Alg.8 η=2 单 poly（`vendor/cbd_eta2/` 自包含拷贝；`SamplePolyCbd2OneRowUb`）；PRF@ws+P0→覆写 src |
| NTT | ntt256 / `ntt_sim_kyber`（**≠ Tag5T**）；输入 = CBD(src) |
| basemul | 标量 Alg.11/12（`vendor/basemul_scalar/`）；双 AIV 各 64 对 |
| INTT | E04/E07 同系矩阵逆 Minv（**≠ Tag5T**） |
| 真算 | L1 SHAKE→CBD；L2 NTT(CBD)→MultiplyNTTs→INTT(Minv) |
| 默认轮次 | `TOY_ROUNDS=3` |
| 验收 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **PASS** |
| kernel wall | ≈ **86.9s** / budget 900；Total tick **644679** |
| Host TRACE | `100/101/105/110/111` ×3 |
| L1 TRACE | `200→210→211→212→220→221→203` ×3 |
| L2 TRACE | AIC `400/401/402`；AIV0 `500→520→530→532→540→542→502`；AIV1 对称 |
| golden | shake `diffs=0/32`；CBD `diffs=0/256`；INTT∘basemul∘NTT(CBD) `diffs=0/256` |
| CrossCore | NTT flag 1/2；INTT flag 5/6；壳层 SET(4)=flag 4 |
| 积木 | SHAKE/Keccak + CBD η=2 + ntt256 + basemul_scalar 自包含；未改原探针 / E01–E07 |
| AscendC API | 复用已登记 `TPipe`/`TBuf`/`TQue`/`DataCopy`/`GetValue`/`SetValue`/`PipeBarrier`/`CrossCore*`/`printf` |
| 根目录 stray dump | 无（stray 已收拢 `sim_log/`） |
| 未测 | SoftSync / OMIT_SET4 / Encrypt / Tag5T 整图 / retracted / 8 行 batch CBD |

**结论**：E07 壳上 SHAKE 后接入真 CBD(η=2)；NTT→basemul→INTT+SET(4) 仍在；≥3 轮 SIM 不挂；SHAKE + CBD + 短链 golden 对拍通过。
