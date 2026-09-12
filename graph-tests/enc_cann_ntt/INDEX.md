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
| EN13 | [EN13-TASK.md](EN13-TASK.md) → [`EN13-encrypt-liboqs-cross/`](EN13-encrypt-liboqs-cross/) | **PASS**（cpu+SIM；`c`≡liboqs max=0；tick≈813k） |
| EN14 | [EN14-TASK.md](EN14-TASK.md) → [`EN14-encrypt-cross-sticky/`](EN14-encrypt-cross-sticky/) | **PASS**（sticky R=8；每轮 c≡liboqs；SIM tick≈6455278） |
| EP01 | [EP01-TASK.md](EP01-TASK.md) → [`EP01-encaps-host-skel/`](EP01-encaps-host-skel/) | **PASS**（Host H/G 壳；c=1568 K=32） |
| EP02 | [EP02-TASK.md](EP02-TASK.md) → [`EP02-encaps-call-encrypt/`](EP02-encaps-call-encrypt/) | **PASS**（真调 Encrypt；自洽 c/K） |
| EP03 | [EP03-TASK.md](EP03-TASK.md) → [`EP03-encaps-device-hash/`](EP03-encaps-device-hash/) | **DEFERRED_HOST** |
| EP04 | [EP04-TASK.md](EP04-TASK.md) → [`EP04-encaps-liboqs-cross/`](EP04-encaps-liboqs-cross/) | **PASS**（c/K≡liboqs；SIM tick≈812819） |
| EP05 | [EP05-TASK.md](EP05-TASK.md) → [`EP05-encaps-sticky-sim/`](EP05-encaps-sticky-sim/) | **PASS**（sticky R=16；每轮 c/K≡liboqs；SIM tick≈12897130） |
| EN15 | — → [`EN15-encrypt-2launch/`](EN15-encrypt-2launch/) | **PASS**（Host launch=**2**；cpu+SIM；c≡liboqs max=0；tick≈898767） |
| EP06 | — → [`EP06-encaps-2launch/`](EP06-encaps-2launch/) | **PASS**（Host launch=**2**；Host FO+Encrypt；c/K≡liboqs；tick≈898662） |

**SIM 里程碑**：EN01–EN09 齐套；EN12 sticky；**EN13–EN14 Encrypt×liboqs**；**EP01–EP05 Encaps（EP03 后置）** — 战役 [`../enc-encaps-cann-ntt-sim/`](../enc-encaps-cann-ntt-sim/INDEX.md) **SIM 强完成**。  
**少 launch（2026-09-12）**：[`../encrypt-encaps-low-launch/`](../encrypt-encaps-low-launch/INDEX.md) — **EN15/EP06** 否决 8-launch 交付形态，锁定 Host=2。  
**NPU**：EN10 + EN11 进程多轮 + EN12 sticky 均不挂（KB S10–S12；X39–X41）；旧编号 EN15/EP10 NPU 仍待用户授权（与本 EN15 2-launch SIM 刀不同）。  

**禁止**：抄 PKE/KEM 算子核；抄 `enc_related/ER0*` 核当模板；未跑任务书要求的 sync_audit 声称完成。  
**旧 hang 线**：[`../enc_related/`](../enc_related/) 只读教训。
