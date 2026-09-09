# AGENT_HANDOFF

**日期**：2026-09-08  
**真机**：实验已 **STOPPED**；请用户 **控制台关机**

---

## ★ 刚发生

- 干净卡 A/B：A=10PASS+2HANG(r11/12)；B=HANG×2+16/16 后按用户意见停掉  
- `run_npu_ab_nohup.sh` 改为默认 **首挂即停**  
- FEEDBACK：`graph_tests/_outbox/FEEDBACK-NPU-AB-CLEAN.md`

## ★ 方法

猎挂默认 `STOP_ON_HANG=1`；要挂率才 `STOP_ON_HANG=0`。
