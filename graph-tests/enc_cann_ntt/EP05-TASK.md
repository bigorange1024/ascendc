# EP05 — Encaps sticky SIM（SIM-only）

> 目录：`graph-tests/enc_cann_ntt/EP05-encaps-sticky-sim/`  
> 建议在 EP04 后。**禁 `-r npu`**。

## 目标

同 session R≥16 SIM；不挂；抽样交叉（若有 liboqs）不红。

## 验收

```bash
EP05_ROUNDS=16 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

（CPU 可先 `EP05_ROUNDS=16`）  
反馈：`tasks/EP05/FEEDBACK.md`。
