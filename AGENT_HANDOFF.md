# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-08（EN12 CPU+SIM+NPU 齐；已推 `bd05d7c`+补丁；NPU 已停）  
> Git：用户已授权本轮 commit/push

## 真机 / 结论

| 项 | 值 |
|----|-----|
| EN12 | **PASS-NOHANG**：CPU+SIM R=16（tick≈12M）；NPU R=32/64 |
| EN10–EN11 | NPU 单轮 / 进程多轮亦 **PASS-NOHANG** |
| 分支 tip | `cursor/cann-ntt-operator-refactor-fe53` · PR #19 |
| NPU | 作业已停（用户关服务器） |

## P0

无强制下一刀；可选旧 Encrypt l18 只读对照。
