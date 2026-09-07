# STATUS — toy-e03-stage-skel-2launch

| 项 | 值 |
|----|----|
| task | E03 / `D-exp-e03` |
| 形态 | Encrypt 骨架 stub：L1 采样 TRACE + Host μ 空 + L2 假代数 + SET(4) |
| SoftSync | **默认不加**（E02 已证极简非必要） |
| 真算 | 无 SHAKE / NTT / 点积 / liboqs |
| 默认轮次 | `TOY_ROUNDS=3` |
| 验收 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **PASS** |
| kernel wall | ≈ **44s** / budget 600 |
| Host TRACE | `100/101/105/110/111` ×3 |
| L1 TRACE | `200→201→202→203` ×3 |
| L2 TRACE | AIC `400/401/402`；AIV0 `500→520→530→540→502`；AIV1 `510→521→531→541→512` |
| magic | `E03TOY01` + `0xE3` |
| 根目录 stray dump | 无 |
| 未测 | OMIT_SET4 / 双 Cube / GATE alone / SoftSync 复测 |

**结论**：新目录可拼装「采样阶段 → Host μ 空 → 代数阶段 + SET(4)」2-launch 骨架，TRACE 可读阶段顺序；≥3 轮 SIM 全绿。
