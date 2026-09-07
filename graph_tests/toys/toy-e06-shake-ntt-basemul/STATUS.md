# STATUS — toy-e06-shake-ntt-basemul

| 项 | 值 |
|----|----|
| task | E06 / `D-exp-e06` |
| 形态 | E05 2-launch 壳 + L1 **真** SHAKE256 + L2 **真** NTT + **真** basemul/MultiplyNTTs + SET(4) |
| SoftSync | 默认不加 |
| SHAKE | 短向量 `abc`→32B；vendor 自包含 |
| NTT | ntt256 / `ntt_sim_kyber`（**≠ Tag5T**） |
| basemul | 标量 Alg.11/12（`vendor/basemul_scalar/`；γ=`alg11_gammas.h`）；双 AIV 各 64 对 |
| 真算 | L1 SHAKE；L2 Split→AIC Mmad×2→Merge+Barrett→**MultiplyNTTs** |
| 默认轮次 | `TOY_ROUNDS=3` |
| 验收 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **PASS** |
| kernel wall | ≈ **66.5s** / budget 900；Total tick **508254** |
| Host TRACE | `100/101/105/110/111` ×3 |
| L1 TRACE | `200→210→211→212→203` ×3 |
| L2 TRACE | AIC `400/401/402`；AIV0 `500→520→530→532→540→502`；AIV1 `510→521→531→533→541→512` |
| golden | shake `diffs=0/32`；MultiplyNTTs `diffs=0/256` |
| CrossCore | NTT flag 1/2；壳层 SET(4)=flag 4 |
| 积木 | SHAKE/Keccak + ntt256 + basemul_scalar 自包含；未改原探针 / E01–E05 |
| AscendC API | 复用已登记 `TPipe`/`TBuf`/`DataCopy`/`GetValue`/`SetValue`/`PipeBarrier`/`CrossCore*`/`printf` |
| 根目录 stray dump | 无 |
| 未测 | SoftSync / OMIT_SET4 / Encrypt / Tag5T / retracted / 向量 B2 Gather |

**结论**：E05 壳上 L2 NTT 之后接入真 basemul；SET(4) 保留；≥3 轮 SIM 不挂；SHAKE + MultiplyNTTs 短向量对拍通过。
