# EP02 — Encaps 真调 Encrypt（SIM-only）

> 目录：`graph-tests/enc_cann_ntt/EP02-encaps-call-encrypt/`  
> 依赖：EP01 形 + EN13 链。**禁 `-r npu`**。

## 目标

Encaps Host：`(K̄,r)←G(m‖H(ek))` → **真调 EN13 形 Encrypt(ek,m,r)→c** → 输出 `c`/`K=K̄`。自洽：长度正确；可选中间 soft golden。

## 验收

cpu + `SIM_DIRECT=1` sim；不挂；`c`/`K` 落盘尺寸对。

反馈：`tasks/EP02/FEEDBACK.md`。
