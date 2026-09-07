# AGENT_HANDOFF

**日期**：2026-09-07（交接 · 请新 Agent 接手）  
**分支**：`cursor/kem-2launch-sticky-1534`（PR #18）  
**上一会话阻塞**：旧 Cloud VM **未注入** `TAILSCALE_AUTHKEY` / `CANNLAB_SSH_KEY`（控制台 Secrets 已有，但只在**新 VM 启动时**注入）→ 用户将**新开 Agent** 接手。

---

## ★ 新 Agent 开场（复制即可）

```text
读 AGENT_HANDOFF.md → docs/engineering/CANNLab接入与远程驱动.md §4/§5.1。
先自查 Secrets（不泄明文）：
  if [ -n "${TAILSCALE_AUTHKEY:-}" ]; then echo "TAILSCALE_AUTHKEY present len=${#TAILSCALE_AUTHKEY}"; else echo MISSING; fi
  if [ -n "${CANNLAB_SSH_KEY:-}" ]; then echo "CANNLAB_SSH_KEY present len=${#CANNLAB_SSH_KEY}"; else echo MISSING; fi
两者均 present 后：按 §4 接入 Tailscale+SSH；确认 CANNLab 已开机并跑过 agent_bootstrap.sh；
然后 bash scripts/cannlab/hang_observe_encaps_decaps.sh（~10min 亲自多轮/交叉 Encaps·Decaps 看粘性挂）。
跑完回写知识库+图谱+qa，控制台关机。禁止打印 Secret 明文。
```

---

## ★ 60 秒上手

1. **拉分支**：`git fetch && git checkout cursor/kem-2launch-sticky-1534 && git pull --ff-only`  
2. Cloud：`bash scripts/clone-thirdparty.sh`（含 liboqs；`ONLY=cannbot-skills` 可补 cannbot）  
3. 读本文件 → [`docs/notes/Encrypt-实机无卡死-知识库.md`](docs/notes/Encrypt-实机无卡死-知识库.md) → [`graph_tests/ENCRYPT_REWRITE_PLAN.md`](graph_tests/ENCRYPT_REWRITE_PLAN.md) §0.1  
4. 联机详版：[`docs/engineering/CANNLab接入与远程驱动.md`](docs/engineering/CANNLab接入与远程驱动.md)  
5. 写 AscendC：cannbot `ascendc-sync-audit` / `ascendc-api-best-practices` + `ascendc-engineering-notes`；**图谱+cannbot 双开**  
6. **真机**：Secrets `TAILSCALE_AUTHKEY` + `CANNLAB_SSH_KEY`；CANNLab WebIDE 一次 `scripts/cannlab/agent_bootstrap.sh`；Agent 侧 Tailscale userspace + `ssh developer@cannlab-npu -p 2222`；**`ASCEND_DEVICE_ID=0`** / `CANNLAB=1`；停机优先**控制台关机**（`sudo kill -TERM 1` 仅兜底且常留「异常」态）

---

## ★ 交接后 P0（按序，勿跳）

| 步 | 做什么 | 成功判据 |
|----|--------|----------|
| **0** | Secrets 自查（上表，**勿打印明文**） | 两 key 均 `present len=…`；若 MISSING → 停，让用户查 scope / 再新开 VM |
| **1** | 用户确认 CANNLab **已开机** + bootstrap 已跑 | `ssh … echo CONNECTED` 成功 |
| **2** | **亲自**跑 Encaps/Decaps 挂取证 | `bash scripts/cannlab/hang_observe_encaps_decaps.sh`（文档 §5.1）；记 `REPORT:` 轮次/挂点 |
| **3** | 回写 | 知识库 §实机事实 + `rg-encrypt-npu-hangfree` F/J + 当日 `qa/2026-09/…` + 本 HANDOFF |
| **4** | 分支 | 若 Encaps/Decaps 粘性可见 → 定最低挂面，再单因子刀；若仍绿 → 可开 **R×N**（`graph_tests/npu_suite/run_rxn_npu.sh`）或 **ENCRYPT-GAP**（见 `BRANCHING.md` B3） |
| **5** | 收工 | **控制台关机/停止**确认停计费 |

**禁止**：ByteDecode / KAT / 正确性结案（挂因未钉前）；抄 stable/frozen 当模板重写；并行多路上板；打印 Secret 明文。

---

## 工作方式（用户锁）

- 本分支 ≠ PR#19（T01–T07 / N0–N10）叙事。  
- Encrypt **从头重拼**：图谱 + cannbot 就地读用；**不** vendor 进 `.cursor/skills/`。  
- `examples/stable` = **main 口径**（只读对照 / 上机取证）；新实验写 `graph_tests/toys/` 等，禁抄 Encrypt 实现。  
- 反馈：用户只打字 `REPORT:`；Agent 经 CANNLab 可自己跑、自己记 log。

---

## 当前真相（勿重复大工程）

| 项 | 状态 |
|----|------|
| 知识库 / 图谱 | v2 cannbot 刷新；`rg_validate` OK；`D-graph-and-cannbot` / `D-next-rxn-or-gap` |
| NPU_SUITE | 单轮 C0–C2 全绿（N7）→ **B3** |
| stable | 已 `checkout origin/main -- examples/stable`；与 main 无 diff |
| CANNLab 文档/脚本 | 已合入；`npu_device_map` 支持 `CANNLAB=1`→device 0；`hang_observe_encaps_decaps.sh` 已加 |
| 上一 Agent | Secrets 自查 **MISSING**（仅 `ASCENDC_GH_PAT`）→ **未连机、未跑 Encaps/Decaps 取证** |
| 正确性 | **后置** |

关键路径：

- 库：`docs/notes/Encrypt-实机无卡死-知识库.md`  
- 图：`docs/rg-encrypt-npu-hangfree.yaml`  
- 计划：`graph_tests/ENCRYPT_REWRITE_PLAN.md`  
- 分支树：`graph_tests/npu_suite/BRANCHING.md`  
- 取证：`scripts/cannlab/hang_observe_encaps_decaps.sh`  
- 纪要：`qa/2026-09/2026-09-07-NPU套件单轮全绿与RxN.md`

---

## 给用户的配合清单（新 Agent 启动前）

1. Cloud Secrets 确认名字精确：`TAILSCALE_AUTHKEY`、`CANNLAB_SSH_KEY`（控制台已有即可）。  
2. **新开** Cloud Agent，checkout **`cursor/kem-2launch-sticky-1534`**（勿沿用旧 VM）。  
3. CANNLab 控制台**启动** 910B3 单卡实例；WebIDE：`TS_AUTHKEY=… bash /mnt/workspace/ascendc/scripts/cannlab/agent_bootstrap.sh`。  
4. 对新 Agent 说：按 `AGENT_HANDOFF` P0 跑 Encaps/Decaps 取证。  
5. 跑完后到控制台**关机/停止**。
