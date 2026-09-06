# STATUS — toy-e05-skel-shake-plus-ntt

| 项 | 值 |
|----|----|
| task | E05 / `D-exp-e05` |
| 形态 | E04 2-launch 壳 + L1 **真** SHAKE256 + L2 **真**单 poly NTT + SET(4) |
| SoftSync | 默认不加 |
| SHAKE | 短向量 `abc`→32B；vendor 自包含拷贝 `shake_xof_kernel`+`keccak_f1600_kernel` |
| **本 NTT golden** | **≠ F203 Tag5T**；语义 = `pass-merged-kyber-mix-ntt256` / `ntt_sim_kyber` |
| 真算 | L1：AIV0 SHAKE256 ProcessInline；L2：AIV Split → AIC Mmad×2 → AIV Merge+Barrett |
| 默认轮次 | `TOY_ROUNDS=3` |
| 验收 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **PASS** |
| kernel wall | ≈ **59.0s** / budget 900；Total tick **459141** |
| Host TRACE | `100/101/105/110/111` ×3 |
| L1 TRACE | `200→210→211→212→203` ×3（真 SHAKE，非 stub） |
| L2 TRACE | AIC `400/401/402`；AIV0 `500→520→530→540→502`；AIV1 `510→521→531→541→512` |
| golden | shake `diffs=0/32`；ntt256 `diffs=0/256`（≠ Tag5T） |
| CrossCore | NTT flag 1/2；壳层 SET(4)=flag 4 |
| 源文件 | ntt 头自包含；SHAKE 在 `vendor/`（未改原 shared / E01–E04） |
| AscendC API | 复用已登记 `TPipe`/`TBuf`/`DataCopy`/`GetValue`/`SetValue`/`PipeBarrier`/`CrossCore*`/`printf`；无新增矢量 API（白名单外未写查阅索引） |
| 根目录 stray dump | 无 |
| 未测 | SoftSync / OMIT_SET4 / Encrypt / Tag5T / retracted |

**结论**：E04 壳上 L1 假采样已换成真 SHAKE256；L2 真 NTT+SET(4) 保留；≥3 轮 SIM 不挂，短向量 SHAKE + ntt256 golden 均对拍通过。
