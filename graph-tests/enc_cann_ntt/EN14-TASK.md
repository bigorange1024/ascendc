# EN14 — Encrypt × liboqs sticky SIM（SIM-only）

> DAG：`D-EXP-EN14`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN14-encrypt-cross-sticky/`  
> **运行**：仅 `cpu` + `SIM_DIRECT=1 sim`；**禁止 `-r npu`**。

---

## 1. 目标

在 **EN13** 的 Encrypt×liboqs 链上，同进程同 session **整链重复 R≥8**（默认 **8**，`EN14_ROUNDS` 可覆盖）：

- 不 recreate stream / 不重 aclInit；缓冲一次分配、R 轮复用  
- 每轮换种子（或 mutate ek/m/coins）后仍 **`c`≡liboqs max=0**（每轮硬门或抽样≥半轮须交叉）  
- 不挂；无 stray dump

---

## 2. 起点与禁令

| 允许 | 禁止 |
|------|------|
| 复制壳：`EN13-encrypt-liboqs-cross/` | 抄 stable encrypt 核、`frozen/**`、ER 核 |
| 参考 EN12 sticky 的「同 session 多轮」编排模式（**勿抄核**） | `-r npu`；并行多路 SIM |

---

## 3. 验收

```bash
cd graph-tests/enc_cann_ntt/EN14-encrypt-cross-sticky
EN14_ROUNDS=8 bash run.sh -r cpu -v Ascend910B4
EN14_ROUNDS=8 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 门禁 | 标准 |
|------|------|
| 不挂 | cpu+sim 正常退出；`round_XX_done.txt` 齐 |
| 正确性 | 每轮或抽样轮 `c` vs liboqs **max=0** |
| 禁 | `-r npu` |

SIM 预算：建议 ≤7200s（对齐 EN12 量级）。

---

## 4. 反馈

写 `graph-tests/enc-encaps-cann-ntt-sim/tasks/EN14/FEEDBACK.md`。
