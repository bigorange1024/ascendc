# STATUS — toy-e04-skel-plus-real-ntt

| 项 | 值 |
|----|----|
| task | E04 / `D-exp-e04` |
| 形态 | E03 2-launch 壳 + L2 **真**单 poly NTT（自包含拷贝 ntt256）+ SET(4) |
| SoftSync | 默认不加 |
| **本 NTT golden** | **≠ F203 Tag5T**；语义 = `pass-merged-kyber-mix-ntt256` / `ntt_sim_kyber`（merged_kyber 单 poly n=256） |
| 真算 | L2：AIV Split → AIC Mmad×2 → AIV Merge+Barrett；L1 仍 stub |
| 默认轮次 | `TOY_ROUNDS=3` |
| 验收 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **PASS**（TRACE + golden） |
| kernel wall | ≈ **54.6s** / budget 900；Total tick **409083** |
| Host TRACE | `100/101/105/110/111` ×3 |
| L1 TRACE | `200→201→202→203` ×3 |
| L2 TRACE | AIC `400/401/402`；AIV0 `500→520→530→540→502`；AIV1 `510→521→531→541→512` |
| golden | `dst.bin` vs `golden.bin`：**diffs=0/256**（ntt256 风格；**≠ Tag5T**） |
| CrossCore | NTT flag 1/2；壳层 SET(4)=flag 4 |
| 源文件 | `aic_func.hpp` / `aiv_func.hpp` / `ntt_vec.hpp` / `basic.hpp` 自包含本目录（未改原 ntt256） |
| 根目录 stray dump | 无 |
| 未测 | SoftSync / OMIT_SET4 / Encrypt / Tag5T |

**结论**：E03 壳上 L2 假 NTT 已换成真单 poly NTT；≥3 轮 SIM 不挂且 ntt256 golden 对拍通过。**本 NTT golden ≠ F203 Tag5T**。
