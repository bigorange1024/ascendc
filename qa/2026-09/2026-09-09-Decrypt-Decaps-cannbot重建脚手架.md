# 2026-09-09 Decrypt/Decaps 收口 · KeyGen NPU×30 关闸 · KB/图推理重写

关键词：**Decrypt/Decaps 重写** · **KeyGen PKE/KEM 重建** · **NPU×30** · **KB/图机制推理重写** · **设备字符串 __gm__**

---

## 1. 目标锁定

| 门禁 | 口径 |
|------|------|
| **正确** | Decrypt `m` / Decaps `K` ≡ **liboqs**；缺库 BLOCKED |
| **反卡死** | 同路径反复 NPU（×30 级）预算内返回；默认 **180s** |
| **关闸** | 设备 Encaps↔Decaps 往返 + 压测（`Q-RT-HANG`） |

Encrypt/Encaps（T22–T24）**已收口，勿重开**。

---

## 2. 本日交付（脚手架）

| 件 | 路径 |
|----|------|
| 积木清单 | `docs/notes/Decrypt-cannbot-rebuild-capability-inventory.md`（DG1–DG7） |
| 工作模式 | `docs/notes/Decrypt-cannbot-rebuild-work-mode.md` |
| KB | `docs/notes/Decrypt-cannbot-rebuild-kb.md`（X12–X16 + Decrypt 拆 launch） |
| DAG | `docs/rg-decrypt-cannbot-rebuild.yaml`（`check_rg_dag` OK，30 nodes） |
| 运营 | `graph-tests/decrypt-rebuild-ops/` |

---

## 3. 实验派发

| ID | 类型 | 内容 |
|----|------|------|
| **DRW-S0A** | 设计 | Decrypt 多 launch 拓扑（cannbot crosscore/sync-audit） |
| **DRW-S0B** | 设计 | Decaps 刀序 + FO/往返计划 |

编码刀 **等 S0 回收**后开 `DRW-D01`。

---

## 4. 禁抄 / 可参考

- **禁**：alg15/21、examples decrypt|decaps、frozen、**T25–T27 源码**  
- **可**：toys/bricks、NTT+编解码探针契约、T22–T24 思路、STATUS/FEEDBACK 判决、cannbot ops

---

## 5. 实验分流更正（同日用户锁定）

| 谁 | 跑什么 |
|----|--------|
| **Subagent** | 设计 · 编码 · **CPU** · **SIM**（本机） |
| **主控** | **全部 NPU**（先请用户启动云机 → SSH/which_npu/`-r npu`/真机 liboqs/×N）；SIM 慢时可 **直接 NPU 同实验**，不必等 SIM |

原因：subagent 连云需逐次授权，用户无暇逐个批。已写入 work-mode / COMMON / RHYTHM / HANDOFF / KB §B3。

---

## 6. 下一棒（已演进）

见文末「进度追加」。

---

## 7. 进度追加（同日）

| 节点 | 结果 |
|------|------|
| D01–D03 | PASS_CPU |
| D04 CPU | PASS；`m≡liboqs_pke_ref` |
| 云机 | `cannlab-npu` `100.84.156.61:2222`；本机 WSL 须 `TS_SOCK=$HOME/.local/share/tailscale/tailscaled.sock` |
| D04 NPU | **PASS**；侧树 `/mnt/workspace/ascendc-drw-d04`；aarch64 现场编 liboqs（勿 scp x86 ref）；`wall_sec=2.652` |
| **Q-DEC-CORRECT** | **closed** |
| K01 G | PASS_CPU + PASS_NPU（`wall_sec=3.258`）；**DG4 关** |
| K02 ReEnc | PASS_CPU + PASS_NPU（并行上板；`wall_sec=3.255`）；**DG5 关** |
| K03 FO | PASS_CPU + PASS_NPU（合法+拒绝≡liboqs）；**Q-DECAPS-CORRECT 关** |
| K04 RT | **PASS_CPU + NPU×30**（ok=30）；修 `AllocSz` unused；**Q-RT-HANG 关** |
| K04 加压收口 | 断前≈93 + 补跑 **7/7** ≈100；用户定：中间用例默认×30、算子×100 |
| 事故 | 空占卡~30min；rsync x86 ref；中间用例空压过长；长跑被 IDLE 掐 |
| 沉淀 | 定稿 note [`MIX-Decrypt-Decaps-反卡死拓扑技术总结.md`](../../docs/notes/MIX-Decrypt-Decaps-反卡死拓扑技术总结.md)；I6–I11 / X17–X20 |
| 写码基线 | **有云机后以 NPU 为准**；**目标未达不停手**（作业完立刻下一刀或沉淀） |
| 进行中→收口 | D04/K01/K02/K03 NPU×30 全绿；K04 smoke×10；**ops_npu_watchdog** 强制实时衔尾（禁干等催） |

| 砖级×30 | D01–D03 全 ok=30；假衔尾已纠；run_controller 强制真进度 |
| KB/DAG 刷新 | §B2.1 为何不挂；§B2.2 KeyGen 潜在卡死；DAG `F-WHY-NO-HANG` / `F-KEYGEN-HANG-RISK`；`Q-KEYGEN-HANG-PROOF` open |

---

## 8. KeyGen 重建战役启动（同日用户锁定）

用户建议并确认：按 Encaps/Decaps 经验 + FIPS203 + 已有知识库/图谱，**重写 PKE/KEM KeyGen**；**禁止直接抄 KeyGen 算子级代码**。

| 项 | 落点 |
|----|------|
| inventory / work-mode / KB | `docs/notes/KeyGen-cannbot-rebuild-*.md` |
| DAG | `docs/rg-keygen-cannbot-rebuild.yaml`（validate OK） |
| 运营 / 实现 | `graph-tests/keygen-rebuild-ops/` · `kg_related/RB-K*` |
| S0 拓扑 | L1 prep → L2a NTT(ŝ/ê) → L2b dot+BE；KEM + L3 AIV-only（KB §B2/B3） |
| 进度 | **P01–P04 PASS_CPU**（P04≡liboqs_pke；npu wait）；**K01 开刀**；云机已关 |
| **工程 KB/DAG** | `ascendc-engineering-kb.md` + `rg-ascendc-engineering.yaml`；viz=**reasoning-graph-skill** `rg-viewer`（已废自造 Cytoscape） |
| Decrypt DAG | `Q-KEYGEN-HANG-PROOF` → closed（移交新战役门禁） |
| P04 回收 | [`KGR-P04`](../../graph-tests/keygen-rebuild-ops/tasks/KGR-P04-pke-full/FEEDBACK.md)；DAG `E-PKE-FULL` verified(CPU) |
| 图谱工具 | 解压 `thirdparty/reasoning-graph-skill-master.zip` → `thirdparty/reasoning-graph-skill/`；见 thirdparty 说明 |
| K01 回收 | [`KGR-K01`](../../graph-tests/keygen-rebuild-ops/tasks/KGR-K01-kem-tail/FEEDBACK.md)；K02 已开 |
| K02 回收 | [`KGR-K02`](../../graph-tests/keygen-rebuild-ops/tasks/KGR-K02-kem-full/FEEDBACK.md) PASS_CPU≡liboqs_kem；**待开机 NPU×30** |
| Local→Cloud 交接 | 2026-09-09 下班：HANDOFF 已写给 Cloud Agent；推送 `chore/thirdparty-add-cannbot-skills` |

### 追加（同日 · Cloud NPU×30 收口）

用户开机 `cannlab-npu` 后 Cloud 上板：

| 档 | 结果 |
|----|------|
| RB-K04 PKE 全链 | NPU×30 pass=30 ≡liboqs_pke |
| RB-K05 KEM tail | 首轮 device `const char*`/`__gm__` 编不过 → `kZPrefixBytes`；CPU+SIM+NPU×30 绿 |
| RB-K06 KEM 全链 | NPU×30 pass=30 ≡liboqs_kem |
| 门禁 | `Q-KEYGEN-HANG` / `Q-KEYGEN-CORRECT` **answered**（`answered_by=F-REBUILD-KEYGEN-NO-HANG`） |
| 证据 | 远端 `/mnt/workspace/keygen-npu-logs/`；artifacts `keygen-npu-x30/` |
| 用机 | 作业后停 keepalive；空闲交平台自动关 |

### 追加（同日 · KB/图谱刷新提交）

用户要求刷新知识库与图谱并提交一版：

| 项 | 落点 |
|----|------|
| 短 KB | `docs/notes/ascendc-engineering-kb.md` §3.4：六算子 stable↔重建 **SIM/NPU launch** 对照表 |
| 图谱 | `F-REBUILD-VS-STABLE-LAUNCH`、`F-REBUILD-KEYGEN-NO-HANG`；`D-SHORT-CROSSCORE-SPLIT` 写入重建基线；`F-DEVICE-STRING-LITERAL-GM` domain→`impl-tech` |
| 校验 | `check_rg_dag.py` + skill `rg_validate` OK；已重渲 `rg-ascendc-engineering.viz.html` |
| skill | 自 Drive 装入 `thirdparty/reasoning-graph-skill/`（gitignore；本机 zip 已落盘） |
| HANDOFF | 指向 KB §3.4 / 图谱节点 |

---

## 9. 工程 KB/图谱章程（同日迭代）

1. 用户：知识≠用法流水；KB 须经得起反复检验。  
2. 用户：换 Agent 只靠 KB+图应能写出正确 AscendC（当前远未达标）。  
3. 用户三条作用：写码经验全收 / 推理入库且冲突刷新 / 指导后续开发——旧「只积卡死」图一条都不占。  
4. **定稿章程**：反卡死是**重点风险**，目标是 **正确∧不卡** 的 KEM 算子；正确性与写码经验**同入库**。

| 落点 | 变更 |
|------|------|
| 短 KB | 双门禁 + 反卡死 + NTT/poly-batch/假绿等写码事实 |
| 图谱 | `D-CORE-KEM-DUAL` / `D-ADMIT-KEM-EXP`；增 `kem-math`/`correctness`；旧 hang-only → inactive；`rg_validate` OK |

### 追加（同日夜 · Launch 压缩计划 · 待 NPU）

用户要求：压低 KeyGen/Decaps launch（旧 stable 更合理；Encaps 已少 launch）；**只做 NPU**、充分用 cannbot-skills、经验及时总结；空闲&gt;3min 会断连；无指示不推送。

| 项 | 结果 |
|----|------|
| 计划 | `graph-tests/launch-reduce-ops/PLAN.md` 全套波次（W0 基线→KG-F1/F2→DC-F1→Decaps→DC-F2） |
| 目标锁 | PKE/KEM KG→**2**；Decaps→**3**；Decrypt 中转 ≤2 优先试 1 |
| 探活 | `which_npu` 失败 → **未启 keepalive**；短周期轮询等开机 |
| Git | 计划已落盘工作区；**未 commit** |
