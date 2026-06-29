# AscendC TQue 与 Pipe/Que — 知识库

**读者**：写 AIV 向量算子、`TPipe`/`InitBuffer` 前查阅  
**教材**：[CANN TQue 简介](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0137.html)  
**Skill**：`.cursor/skills/ascendc-engineering-notes/SKILL.md`

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | TQue / TBuf / TPipe 语义 |
| §2 | 硬件限额（event 槽） |
| §3 | 分配原则与模式 |
| §4 | 故障分类 |
| §5 | 写码检查单 |
| §6 | 附录：本仓案例 |

---

## 1. 语义模型

| 机制 | 角色 |
|------|------|
| **TPipe** | 单 kernel 内 UB 的 **总分配器** |
| **TQue** | 队列 + **同步 event**；`AllocTensor/EnQue/DeQue` |
| **TBuf** | 同 pipe 上 UB 切片；**不占 Que event 槽** |

**误区**：TQue ≠「每个 GM 输入一个」。中间向量量应优先 **TBuf + GetWithOffset**。

### 1.1 不变量：单算子单 TPipe

```text
class AivFoo {
  TPipe pipe;
  void Init() { pipe.InitBuffer(...); }  // 仅一次
};
```

**禁止**：`for (p) { AivFoo foo; foo.Init(); }` — 重复 `AllocEventID` → 挂死。

详见 [merged-kyber-poly-batch-NTT技术总结.md](merged-kyber-poly-batch-NTT技术总结.md) §2.1。

### 1.2 融合：单 TPipe 地图

NTT + 内积融合 = **一个 TPipe 规划整张 UB**；子算子另建 TPipe 导致 **静默别名**（见 [F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md) §3.1）。

---

## 2. 硬件限额（Atlas A2 / 910B）

| 芯片族 | 每 TPosition 最多 TQue |
|--------|------------------------|
| Atlas 训练 | 4 |
| **A2 / 910B 等** | **8** |

- `InitBuffer(TQue, num, len)`：`num=2` 为 double buffer，占 **2** 槽  
- 超限 → `AllocEventID: current id is 8, max … 8` / SIGABRT  

**VECCALC 上 TBuf 不计入** 8，但仍占 UB **容量**。

---

## 3. 分配原则

### 3.1 模式 P-que-1：少 Que，多 TBuf

| 需要 | 用 |
|------|-----|
| MTE↔Vector 握手边界 | 少量 TQue（常 1–2） |
| 向量中间量 a0,b0,c0,… | TBuf scratch 切片 |
| GM 输入 | `DataCopy` 到 scratch，不必每路 TQue |

**反例**：行 18 首版 14 个 TQue → 超 8 崩溃。

### 3.2 模式 P-que-2：冲突区独立 TQue

scratch 偏移图画清后，若大块与循环临时区 **重叠**，用 **独立 TQue** 承载（内积 half 路线教训）；活跃全 poly 路线 UB 更简单。

### 3.3 depth / num

单趟 `CopyIn→Compute→CopyOut`：`TQue<…,1>` + `InitBuffer(que,1,bytes)`。非必要勿 double buffer。

### 3.4 TPosition

| Position | 用途 |
|----------|------|
| VECIN | 搬入后队列 |
| VECOUT | 写出前队列 |
| VECCALC | 计算；TBuf 常在此 |

VECIN 的 8 槽 **独立统计**。

---

## 4. 故障分类

| 现象 | 类 | 处理 |
|------|-----|------|
| AllocEventID / max 8 | TQue 过多 | 合并 TBuf；减 Que |
| CPU 慢 / SIM 挂死无 assert | 循环内 new TPipe | 单 pipe + 扩大 tile |
| FreeTensor abort | 次生；先查 Init 个数 | 修地图 |
| 融合后全系错 | 双 TPipe 别名 | 单地图 |

---

## 5. 写码检查单

- [ ] 本 AIV **一个** TPipe，`Init` 只一次？  
- [ ] VECIN TQue（含 num）≤ 8？  
- [ ] 中间量优先 TBuf？  
- [ ] `for (poly)` 内构造带 TPipe 的类？（禁止）  
- [ ] 与 [DataCopy 知识库](ascendc-DataCopy与数据搬运知识库.md) 一致：CopyIn 目标可是 TBuf  

---

## 6. 附录：本仓案例

| 案例 | 要点 | 文档 |
|------|------|------|
| batch NTT 循环 TPipe | event 耗尽 | [merged-kyber-poly-batch-NTT技术总结.md](merged-kyber-poly-batch-NTT技术总结.md) |
| Stage3 ~7 TQue 贴限 | calc_f 用 TBuf | frozen tag5t |
| 行 18 十四 TQue | →2 TQue+scratch | frozen alg13 |
| UB 融合 | dotScratch 分区 | [F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md) |

---

*2026-06-18：重构为原理优先；去除 docs/research 引用。*
