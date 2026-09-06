# STATUS — toy-e07-shake-ntt-basemul-intt

| 项 | 值 |
|----|----|
| task | E07 / `D-exp-e07` |
| 形态 | E06 2-launch 壳 + L1 **真** SHAKE256 + L2 **真** NTT + **真** basemul + **真** INTT + SET(4) |
| SoftSync | 默认不加 |
| SHAKE | 短向量 `abc`→32B；vendor 自包含 |
| NTT | ntt256 / `ntt_sim_kyber`（**≠ Tag5T**） |
| basemul | 标量 Alg.11/12（`vendor/basemul_scalar/`；γ=`alg11_gammas.h`）；双 AIV 各 64 对 |
| **INTT** | **E04 同系**矩阵逆：Host `Minv=M^{-1}(mod q)`→`Minv4.bin`；设备 Split→AIC Mmad(Minv)×2→Merge+Barrett + 半区 canonical Barrett |
| **INTT 语义** | **≠ Tag5T**（非 polyvec8 LUT；只读参考 `pass-fix-f203-stage123-ntt-intt-polyvec8-vec`） |
| 真算 | L1 SHAKE；L2 NTT→**MultiplyNTTs**→**INTT(Minv)** |
| 默认轮次 | `TOY_ROUNDS=3` |
| 验收 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **PASS** |
| kernel wall | ≈ **96.4s** / budget 900；Total tick **584061** |
| Host TRACE | `100/101/105/110/111` ×3 |
| L1 TRACE | `200→210→211→212→203` ×3 |
| L2 TRACE | AIC `400/401/402`；AIV0 `500→520→530→532→540→542→502`；AIV1 `510→521→531→533→541→543→512` |
| golden | shake `diffs=0/32`；INTT∘basemul `diffs=0/256` |
| CrossCore | NTT flag 1/2；INTT flag 5/6；壳层 SET(4)=flag 4 |
| 积木 | SHAKE/Keccak + ntt256 + basemul_scalar 自包含；未改原探针 / E01–E06 |
| AscendC API | 复用已登记 `TPipe`/`TBuf`/`DataCopy`/`GetValue`/`SetValue`/`PipeBarrier`/`CrossCore*`/`printf` |
| 根目录 stray dump | 无（stray 已收拢 `sim_log/`） |
| 未测 | SoftSync / OMIT_SET4 / Encrypt / Tag5T 整图 / retracted |

**结论**：E06 壳上 basemul 之后接入真 INTT（ntt256 同系 Minv；**≠ Tag5T**）；SET(4) 保留；≥3 轮 SIM 不挂；SHAKE + INTT∘MultiplyNTTs 短向量对拍通过。
