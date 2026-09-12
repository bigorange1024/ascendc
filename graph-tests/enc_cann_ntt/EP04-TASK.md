# EP04 — Encaps × liboqs 交叉（SIM-only · 强完成）

> 目录：`graph-tests/enc_cann_ntt/EP04-encaps-liboqs-cross/`  
> **禁 `-r npu`**。

## 目标

同种子约定下 `c`/`K` 与 liboqs ML-KEM-1024 Encaps **逐字节**（或登记允许域）。  
Glue：`scripts/liboqs_kem_fixture.py` / `liboqs_kem_ref` / `liboqs_kem_vs_ascendc*`。

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

`c` 与 `K` vs liboqs **max=0**。

反馈：`tasks/EP04/FEEDBACK.md`。
