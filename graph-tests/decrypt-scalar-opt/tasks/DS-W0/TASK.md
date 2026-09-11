# DS-W0 · 基线复测（H0）

## 目标

不改代码；在目标板上复测 `RB-D09-decrypt-1launch`：

1. `bash run.sh -r npu -v Ascend910B4` 单轮 PASS  
2. NPU×5 换 SEED 冒烟  
3. ops-profiling Σ Task Duration + msopprof PipeUtilization  

写入本目录 `FEEDBACK.md`。与 `qa/active_npu_perf_summary.md` 偏差 &gt;5% → **停**，先查频点/设备/污染。

## 命令

```bash
cd graph-tests/dec_related/RB-D09-decrypt-1launch
bash run.sh -r npu -v Ascend910B4
# ×5 / profiling：按 PLAN §5 与 round_003 同口径
```

## 完成定义

`FEEDBACK.md` 填齐 µs、scalar%、Freq、日志路径；QUEUE 中 DS-W0 → 完成。
