# Encrypt + Encaps × cann-ntt · SIM-only 重写战役

> **主目标**：用 **迁入 cann-ntt** 的 MIX NTT/INTT，搭 **不卡死** 的 ML-KEM-1024 **Encrypt（Alg.14）→ Encaps（Alg.16/20）**。  
> **本阶段强制**：**只跑 CPU + `SIM_DIRECT=1` sim**；**禁止 NPU / 禁 keepalive 占卡**（用户休息期间不得空转烧板）。  
> **工作模式**：主控定刀 + cannbot-skills **设计** + 推理图谱挂账 + subagent 编码；一刀一目录、先 SIM 绿再谈上机。

| 文档 | 路径 |
|------|------|
| **全套计划** | [`PLAN.md`](PLAN.md) |
| **上板/上 SIM 队列** | [`QUEUE.md`](QUEUE.md) |
| 能力清单（Encrypt 继承） | [`docs/notes/Encrypt-cann-ntt-capability-inventory.md`](../../docs/notes/Encrypt-cann-ntt-capability-inventory.md) |
| 工作模式（继承+本战役补丁） | [`docs/notes/Encrypt-cann-ntt-workmode.md`](../../docs/notes/Encrypt-cann-ntt-workmode.md) · 本战役 §PLAN「SIM-only 门禁」 |
| KB（Encrypt 线） | [`docs/notes/Encrypt-cann-ntt-kb.md`](../../docs/notes/Encrypt-cann-ntt-kb.md) |
| **本战役推理图谱** | [`docs/rg-enc-encaps-cann-ntt-sim.yaml`](../../docs/rg-enc-encaps-cann-ntt-sim.yaml) |
| 既有 Encrypt 不挂线（基线） | [`../enc_cann_ntt/`](../enc_cann_ntt/) EN01–EN12 |

## 继承结论（勿重做）

`enc_cann_ntt` EN01–EN12 已证明：**Host 多段 + cann-ntt NTT/INTT** 在 SIM（及曾授权的 NPU）上 **可不挂**。  
**EN13**（2026-09-11）：Encrypt `c` ≡ liboqs **max=0**（cpu+SIM）。  
**SIM 强完成**（同日）：EN14 sticky + EP01–EP05 Encaps（EP03=`DEFERRED_HOST`）全绿；见 [`QUEUE.md`](QUEUE.md)。

## 本战役波次（摘要）

| 波 | 内容 | 运行态 | 状态 |
|----|------|--------|------|
| **W0** | 文档/图谱/队列锁盘；核对 EN07/09 基线可复现 SIM | **SIM-only** | **PASS** |
| **W1** | Encrypt 权威交叉（liboqs）· sticky · SIM | **SIM-only** | **PASS**（EN13/EN14） |
| **W2** | Encaps 积木（G/H/m、调 Encrypt、输出 c/K）· SIM | **SIM-only** | **PASS**（EP01/EP02；EP03 后置） |
| **W3** | Encaps 贯通 + sticky 多轮 · SIM | **SIM-only** | **PASS**（EP04/EP05） |
| **WN** | NPU 冒烟 / sticky | **默认封锁**；仅用户清醒授权后开 | **BLOCKED** |

## 硬禁令

- 休息时段：**禁止** SSH 上板、**禁止** keepalive、**禁止**「空等下一刀」占卡。  
- 禁抄 `examples/**/encrypt*|encaps*`、`frozen/**` 核、旧 `enc_related/ER0*` 核。  
- NTT/INTT **必须**用迁入 cann-ntt；禁胖 MIX「NTT+GATE+matvec+INTT」单核。  
- 无用户授权：**不** commit / push / 开分支。
