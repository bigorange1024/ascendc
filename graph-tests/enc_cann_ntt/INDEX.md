# graph-tests/enc_cann_ntt

Encrypt **Host 多段编排 + 迁入 cann-ntt NTT** 试验场（ML-KEM-1024；主目标不卡死）。

| 文档 | 路径 |
|------|------|
| 能力清单 | [`docs/notes/Encrypt-cann-ntt-capability-inventory.md`](../../docs/notes/Encrypt-cann-ntt-capability-inventory.md) |
| 工作模式 | [`docs/notes/Encrypt-cann-ntt-workmode.md`](../../docs/notes/Encrypt-cann-ntt-workmode.md) |
| 知识库 | [`docs/notes/Encrypt-cann-ntt-kb.md`](../../docs/notes/Encrypt-cann-ntt-kb.md) |
| DAG | [`docs/rg-encrypt-cann-ntt.yaml`](../../docs/rg-encrypt-cann-ntt.yaml) |

| 刀 | 任务书 / 目录 | 状态 |
|----|----------------|------|
| EN01 | [EN01-TASK.md](EN01-TASK.md) → [`EN01-kem256-ntt-port/`](EN01-kem256-ntt-port/) | **PASS-NOHANG** |
| EN02 | [EN02-TASK.md](EN02-TASK.md) → [`EN02-ntt-intt-2launch/`](EN02-ntt-intt-2launch/) | **PASS-NOHANG** |
| EN03 | [EN03-TASK.md](EN03-TASK.md) → [`EN03-encrypt-host-skel/`](EN03-encrypt-host-skel/) | **PASS-NOHANG** |
| EN04 | [EN04-TASK.md](EN04-TASK.md) → [`EN04-matvec-realbrick/`](EN04-matvec-realbrick/) | **PASS-NOHANG** |
| EN05 | [EN05-TASK.md](EN05-TASK.md) → [`EN05-prep-sample-realbrick/`](EN05-prep-sample-realbrick/) | **PASS-NOHANG** |
| EN06 | [EN06-TASK.md](EN06-TASK.md) → [`EN06-pack-compress-realbrick/`](EN06-pack-compress-realbrick/) | **PASS-NOHANG** |
| EN07 | [EN07-TASK.md](EN07-TASK.md) → [`EN07-pipeline-wired/`](EN07-pipeline-wired/) | **PASS-NOHANG** |
| EN08 | [EN08-TASK.md](EN08-TASK.md) → [`EN08-wired-sticky-rounds/`](EN08-wired-sticky-rounds/) | **PASS-NOHANG** |
| EN09 | [EN09-TASK.md](EN09-TASK.md) → [`EN09-samplentt-device/`](EN09-samplentt-device/) | **PASS-NOHANG** |
| EN10 | [EN10-TASK.md](EN10-TASK.md) → NPU 跑 EN09 | **PASS-NOHANG**（910B3/dev4） |
| EN11 | [EN11-TASK.md](EN11-TASK.md) → NPU 连续多轮 EN09 | **PASS-NOHANG**（EN11e R=200 soft=0 hang=0） |
| EN12 | [EN12-TASK.md](EN12-TASK.md) → [`EN12-samplentt-sticky/`](EN12-samplentt-sticky/) | **PASS-NOHANG**（CPU+SIM sticky R=16；NPU sticky R=32/64；EN12d 加压） |

**SIM 里程碑**：EN01–EN09 齐套；EN12 sticky R=16 SIM 不挂（kernel≈1881s）。  
**NPU**：EN10 + EN11 进程多轮 + EN12 sticky 均不挂（KB S10–S12；X39–X41）。  

**禁止**：抄 PKE/KEM 算子核；抄 `enc_related/ER0*` 核当模板；未跑任务书要求的 sync_audit 声称完成。  
**旧 hang 线**：[`../enc_related/`](../enc_related/) 只读教训。
