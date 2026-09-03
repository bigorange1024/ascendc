# 干净重写 Encrypt — 设计原则（从推理图谱导出）

> 主控维护。落地目录目标：`ascendc-tests/fix-encrypt-clean-hostmu-2launch/`（PHASE 由 TASK 推进）。  
> 对照图谱：`docs/rg-kem-encrypt-hang.yaml`。  
> **最终仍服务**：NPU 上 Encrypt 不卡死且正确；本树可与 Host 折 μ 改法一并上机。

---

## 1. 必须遵守的已证知识

| 节点 | 设计约束 |
|------|----------|
| `J-empty-trace-aic-wait4` verified | skipNtt：AIC 入口 `Wait(4)` 前，AIV **必须**能到达双 AIV `SET(4)`；禁止把重前缀挡在 SET 前而无旁路 |
| `F-omit-set4-hangs-sim` | 缺 SET(4) ⇒ 挂；实现与评审以「SET(4) 可达性」为第一不变量 |
| `F-host-mu-ok-sim` / `F-stable-host-mu-sim-pass` | **μ 折叠默认在 Host**；设备 skipNtt **不出现** `PrefixEmbedMu` |
| `D-forbid-syncall-while-wait` | 禁止 AIC Wait 中 SyncAll |
| `D-softsync-follow-decrypt` | 若需 AIV 汇合，只跟既有 SoftSyncArrive 定式 |
| `D-reject-correctness-antipattern` | 禁止滥 Host launch、碎写 GM 当正解 |
| `D-verify-sim-for-npu` | 迭代只认 SIM；不沉淀 CPU |

## 2. 已证伪（不要写回设计）

- 仅拆双 Cube / 仅加 launch 数消粘  
- stub 下仅 1/3、仅合法 GATE、仅更大 Cube 当作挂因充分条件  
- AIC Wait 时 SyncAll、自造 SoftSync 双向汇合  

## 3. 目标拓扑（干净 Encaps Encrypt 段）

```
Host:
  Launch-1  prep+NTT（一轮 Cube；无设备 μ）
  HostFold: e2 += μ (mod q)     # 默认、非调试开关
  Launch-2  l18 skipNtt：at_jp → SET(4) → GATE → INTT → pack
Device L2:
  AIC: Wait(4) → Set(8) → Wait(1) MMAD Set(3) …
  AIV: （无 PrefixEmbed）at_jp → 双 AIV Set(4) → Wait(8) → …
```

## 4. 分阶段交付

| PHASE | 内容 | 验收 |
|-------|------|------|
| **P0** | 目录骨架 + Host 2-launch + Host μ + 设备 skipNtt 无 PrefixEmbed；stub at_jp/INTT 可先用轻量 | SIM 结束 + magic/标记 |
| **P1** | 接入真实 at_jp / INTT / pack（`library/shared`，禁 frozen） | SIM 与 golden I/O 或分阶段 cmp |
| **P2** | prep+NTT 真实化；与 stable Encaps I/O 对齐 | SIM 对拍；再请用户 NPU |

## 5. 与 stable Host 折 μ 的关系

- stable 上的 `F203_HOST_FOLD_MU` 是**热修**。  
- 本干净树把同一约束写成**默认结构**，避免「调试开关忘了开」类回归。  
- 上机时可同时测：stable 热修 + 本 clean 探针。

## 6. 非目标

- 本阶段不解决 Decaps K=131（见 `docs/rg-kem-decrypt-k131.yaml`）。  
- 不引入 correctness 反模式。  
