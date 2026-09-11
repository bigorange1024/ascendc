# EN11 — NPU 贯通链连续多轮（防空转 + 粘性加压）

> DAG：`D-EXP-EN11`  
> 基线：远程 EN09 链 @910B3/`ASCEND_DEVICE_ID=4`  
> 目标：连续 ≥4 轮不挂；保持 NPU 忙碌（空闲&lt;4min）

## 验收

- 同机连续跑通 SampleNTT→…→Pack ≥4 次  
- 无 SynchronizeStream 卡死 / timeout 124  
- 日志：`/mnt/workspace/en11*_npu_loop.log` · 本地 `/opt/cursor/artifacts/EN11d-npu-loop.log`

## 结果（2026-09-08）

| 轮次 | 结果 |
|------|------|
| EN11a/b | 失败：缺 `.so` 路径 / 错 BIN（X40） |
| EN11d | **PASS-NOHANG** R=8 · wall≈2–3s/轮 |
| EN11e | R=200 加压（`verify_result.py`） |

正确性：尽量每轮对拍；失败标 soft-fail，主门禁仍为不挂。
