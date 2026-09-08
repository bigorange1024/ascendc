# Encrypt 重建 · 任务总队列

> **收口（2026-09-08）** · 日志：`/mnt/workspace/encrypt-rebuild-npu-loop.log` · 压测：`encrypt-rebuild-hang-stress.log`

## 已关闭

| ID | 状态 |
|----|------|
| T01–T22 | 双绿（含设备 G Encaps） |
| T23 | 双绿 · Encaps×liboqs CROSS |
| T24 | 双绿 · Encaps→liboqs Decaps RT；**×30 不挂压测 ok=30** |
| Q-ULT | **answered** |

## 活跃 / 可选

| 轨 | 状态 |
|----|------|
| NPU | T06 sticky 保活（禁刷已绿 T23/T24） |
| 可选下一刀 | 设备 Decaps（若继续）；否则停战役 |
