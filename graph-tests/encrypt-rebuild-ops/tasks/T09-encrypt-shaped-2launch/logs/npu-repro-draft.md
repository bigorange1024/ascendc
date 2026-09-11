# NPU 复现步骤草稿（T09 · 供主控 N02）

> Subagent 本机未跑 NPU；以下供主控 Tailscale/SSH 上板。

## 前置

1. rsync 本目录至远端（排除 `build/ out/ input/ output/ OPPROF_* sim_log/`）
2. `ASCEND_DEVICE_ID=0`；上板前 `rm -rf build out input output`
3. 清产物防 X8 假红；勿脏杀 timeout 进程

## 命令

```bash
cd graph-tests/enc_related/RB-T09-encrypt-shaped-2launch
rm -rf build out input output
export ASCEND_DEVICE_ID=0
# 远端 SOC 常为 Ascend910B3
bash run.sh -r npu -v Ascend910B3
```

## 期望

- exit 0；`[PASS_SYNC]` +（理想）`[PASS_IO]`
- TRACE：PREP_DONE → NTT SET1+WAIT3 → SET4 → INTT → PACK_DONE
- 双 Cube `mat_c_*` 非全 0；`c.bin` 1568B

## 若挂

见用例 `trace_map.md` 卡点假设；对照 cannbot `deadlock-triage.md`。禁改 flag 5/7。
