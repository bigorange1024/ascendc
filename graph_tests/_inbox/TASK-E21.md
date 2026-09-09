# TASK-E21 — SIM/静态：l18 末段 CrossCore 缺口审计（只读 + audit）

**禁 NPU；禁改 Encrypt 业务码**

## 目的

用文字 + cannbot 审计收紧「最后 CrossCore → Host Sync」窗，不写新业务。

## 做

1. **只读** `examples/stable/.../kem-encaps-k4/compute/f203_encrypt_l18_l19_kernel.cpp`  
   - 列出 AIC 最后一次 `FsmSet/FsmWait` 行号与状态  
   - 列出此后 AIV 仍执行的步骤（mod_q / V_DONE Mark / `tail_pack_shard_gm`）  
   - 结论表：末段是否 **零 CrossCore**  
2. 对 **同一文件**（或抽出的 FSM 包装若 audit 需要）跑：
   ```bash
   python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
     examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/compute/f203_encrypt_l18_l19_kernel.cpp \
     --check all --format json > /opt/cursor/artifacts/e21-l18-sync-audit.json || true
   ```
   若路径/包装导致假阳性，在 FEEDBACK 注明（对照 E17 SYNC-03）。  
3. 可选轻量：若 encaps 用例已有 `run.sh -r sim` 且环境就绪，**仅 1 轮** SIM 确认默认路径仍绿（失败则记阻塞，勿硬扛多轮）。禁 TRACE 默开。

## 交付

- `graph_tests/_outbox/FEEDBACK-E21.md`（含末段时间线表 + audit 摘要）  
- artifacts 下 json  

## 禁

- 改 kernel / 开 TRACE 默认 / NPU / commit  
