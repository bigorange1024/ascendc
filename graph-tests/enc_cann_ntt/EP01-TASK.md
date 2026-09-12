# EP01 — Encaps Host 壳（SIM-only）

> 目录：`graph-tests/enc_cann_ntt/EP01-encaps-host-skel/`  
> **禁 `-r npu`**。

## 目标

Host：读 `ek`；采样 `m`；算 `H(ek)`、`G(m‖H(ek))`→`(K̄,r)`；**桩** Encrypt（可用固定长度假 `c` 或调用 Host Python）写出 `c`(1568)/`K`(32)。

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

不挂；长度：`c=1568`、`K=32`、`ek` 合法。可无设备核（纯 Host 亦可，但须能走用例 `run.sh` 壳）。

反馈：`enc-encaps-cann-ntt-sim/tasks/EP01/FEEDBACK.md`。
