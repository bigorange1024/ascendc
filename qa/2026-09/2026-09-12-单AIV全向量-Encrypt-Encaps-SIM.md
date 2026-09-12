# 2026-09-12 · 单 AIV 全向量 Encrypt/Encaps（SIM）

## 结论（主控独立复验）

- **Encrypt（AE-E2）PASS**：单 AIV、单 launch、`c`≡liboqs max=0；SIM tick≈1200108。
- **Encaps（AE-P2）强 PASS**：同 launch 内 DEVICE_FO（SHA3 H/G）+ DEVICE_CBD + Encrypt；`c/K`≡liboqs；SIM tick≈1384945。
- 积木：AV01 系向量 NTT；新建向量 INTT；向量 matvec/dot；pack；设备 Keccak FO。
- 残差：Â 的 SampleNTT×16、t̂ ByteDecode₁₂ 仍可 Host；未上 NPU。
- 图谱/KB 已回写；**未** git 分支/commit/push。

## 本轮再验（会话续跑，无代码改动）

| 用例 | CPU | SIM | tick |
|------|-----|-----|------|
| AE-E | PASS c max=0 | PASS | ≈1200017 |
| AE-P | PASS c/K max=0 | PASS | ≈1384867 |

日志：`/opt/cursor/artifacts/aiv-kem-ae-{e,p}-{cpu,sim}.log`；用例根无 stray dump。  
用户指令：继续到 Encrypt/Encaps 正确为止；**禁擅自分支/推送**——遵守，未 commit。

## 全 AscendC（L8 加锁后）

Host 不再预喂 Â/t̂/y/e。主控复验：

| 用例 | 输入 | CPU | SIM tick |
|------|------|-----|----------|
| AE-E | ek\|m\|coins | c max=0 | ≈1885458 |
| AE-P | ek\|m | c/K max=0 | ≈1977711 |

资产清单：`graph-tests/aiv-kem-vector-sim/ASSETS.md`。  
架构收益（用户确认）：单 AIV 可串多例、无组件同步；多 AIV = 多任务并行。

## 假绿三问

1. golden 同源？否——权威 liboqs fixture。  
2. 权威交叉？是——cpu+SIM 均 max=0。  
3. 跨核 Sync？AIV-only，无 CrossCore；sync_audit 红线 0。

## NPU sticky/perf（CANNLab · 2026-09-12 夜 · Ascend910B3）

主机：`cannlab-npu` / `100.104.187.44:2222`；`ASCEND_DEVICE_ID=0`；远端 `b26baf1`。  
作业：`/mnt/workspace/jobs/kem-sticky-perf_20260912_160608` → 本地 `/opt/cursor/artifacts/npu-kem-sticky-perf/`。

| 用例 | 轮次 | 结果 | hangs | 端到端 wall（含编译） | kernel wall（稳态） |
|------|------|------|-------|----------------------|---------------------|
| AE-E Encrypt | 3 | PASS c≡liboqs max=0 | 0 | 26–33s | ≈2.25–9.6s |
| AE-P Encaps | 3 | PASS c/K≡liboqs max=0 | 0 | 27–28s | ≈2.25–3.3s |
| EN13 Encrypt×liboqs | 3 | PASS c max=0 | 0 | 27–28s | ≈2.25–3.3s |
| EP04 Encaps×liboqs | 3 | PASS c/K max=0 | 0 | 27–28s | ≈2.25–2.7s |
| EN14 sticky R=8 | 2×8 | PASS 每轮 max=0 | 0 | 30–31s | ≈2.25–2.7s |
| EP05 sticky R=16 | 2×16 | PASS 每轮 c/K max=0 | 0 | 34s | ≈2.25s |

**结论**：AIV 与 cann-ntt 两线 NPU **无挂死**；交叉全绿。注意：远端须 **aarch64** 重编 `liboqs_*_ref`（勿 scp x86 二进制）。

> **纠偏（同日用户）**：防挂死核心是 **外层多跑**，至少 **×30**；上次 AE-E/AE-P 只跑 3 轮不够。「sticky」指 EN14/EP05 **用例内**粘滞多轮，≠ 外层防挂死次数。连不上时须立刻反馈，禁止空等——已写入 `docs/engineering/CANNLab接入与远程驱动.md` §5.1、`AGENTS.md` 硬门禁、`scripts/cannlab/lib_ssh.sh`（最多 2 次短试）。

## Agent 纪律补记（用户钉死 · 同日）

| 禁止 | 要求 |
|------|------|
| 傻等/空等服务器 | 硬超时短探测，失败马上说「连不上」 |
| 无限重连还不汇报 | `cannlab_ssh_try`≤2；打印 `VERDICT=DISCONNECTED` |
| 只有 `127.0.0.1:2222` 仍死磕 | 请用户重 bootstrap；等下一句再测 |

## 少 launch 纠偏（同日用户）

- 历史收口：Encrypt/Encaps **Host 2 launch**；Decrypt **1 launch**。  
- 真机 cann-ntt 接线版出现 **8 次 kernel launch** → **不是正确交付形态**。  
- 已新开战役：[`graph-tests/encrypt-encaps-low-launch/`](../../graph-tests/encrypt-encaps-low-launch/INDEX.md)。

## 少 launch · SIM 全实验收口（同日夜 · 用户睡眠授权自主）

锁定：**AIV launch=1**；**cann-ntt launch=2**；仅 cpu+sim。

| 用例 | launch | cpu+SIM | 交叉 |
|------|--------|---------|------|
| AE-E-encrypt | 1 | PASS | c≡liboqs max=0 |
| AE-P-encaps | 1 | PASS | c/K≡liboqs max=0 |
| EN15-encrypt-2launch（新） | 2 | PASS | c≡liboqs max=0 |
| EP06-encaps-2launch（新） | 2 | PASS | c/K≡liboqs max=0 |

证据：`/opt/cursor/artifacts/low-launch-sim/`。战役 QUEUE **全 DONE**。未 commit（等授权）。
