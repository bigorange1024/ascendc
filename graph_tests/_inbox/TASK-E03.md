# TASK E03

## 元数据
- task_id: E03
- issued_at: 2026-09-06T07:33:00Z
- deadline_min: 35
- max_retries: 1
- silent_hang_min: 8
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e03, F-e02-softsync-set4-sim-pass, D-host-mu-default, D-use-blocks, D-short-experiments]
- hypothesis_under_test: [D-exp-e03]
- write_graph: no
- concurrency: solo

## 目标
新目录做出 **Encrypt 形态骨架**（无真密码学）：L1=采样形态 stub 阶段齐；Host 可空 μ；L2=代数形态 stub 阶段齐 + SET(4)；同进程 ≥3 轮 SIM 全跑完。

## 白名单
- **仅** `graph_tests/toys/toy-e03-stage-skel-2launch/`
- 可只读仿 E01/E02 工程壳；**禁止改** E01/E02/冻结/stable/图谱/知识库
- 禁止抄 Encrypt 业务；禁止真 SHAKE/NTT/对拍 liboqs
- 禁止复测双 Cube / GATE alone / OMIT_SET4 发现

## 阶段 TRACE（建议号段；可微调但须写入 TRACE.md）

**L1 采样形态（stub，只打点）**  
- 200 进入 L1 / 201 假 seed expand / 202 假 CBD/noise / 203 L1 将返回  

**Host**  
- 100→101 L1；105 Host μ 空操作（可选）；110→111 L2  

**L2 代数形态（stub + SET4）**  
- 400 AIC 入；401 Wait(4) 前；402 Wait 后  
- 500/510 AIV 入；520/521 假 NTT；530/531 假点积；540/541 假 INTT；502/512 SET(4)  
- SoftSync：**本刀默认不加**（E02 已证极简非必要）；若加须说明理由

## 验收 PASS
- [ ] ≥3 轮 2-launch SIM 绿
- [ ] TRACE 能看出采样阶段→代数阶段顺序（缺段=FAIL）
- [ ] 无真算/无 Encrypt 抄码
- [ ] TRACE.md + STATUS.md + `_outbox/FEEDBACK-E03.md`（support D-exp-e03）
- [ ] ≤35min

## 必读
`SUBAGENT_RULES.md`；E01/E02 FEEDBACK；ascendc-engineering-notes SKILL
