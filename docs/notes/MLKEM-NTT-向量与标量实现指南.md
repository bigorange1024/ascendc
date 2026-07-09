# ML-KEM NTT — 向量与标量实现指南

**读者**：在 AscendC 上实现 2s1e 集成线的开发者  
**数学契约**：[MLKEM-NTT-实现总结.md](MLKEM-NTT-实现总结.md)  
**活跃探针**：`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`、`exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4`（预研）  
**frozen 治理**：[研究路线与frozen治理.md](研究路线与frozen治理.md)

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | 端到端数据面（2s1e 语义） |
| §2 | 向量/标量分工原则 |
| §3 | Gather 禁令的 **范围** |
| §4 | 模运算与 golden 解耦 |
| §5 | UB 融合不变量 |
| §6 | 验证与性能方法论 |
| §7 | 附录：开关、探针、冻结表 |

---

## 1. 端到端数据面（2s1e）

### 1.1 逻辑对象

KeyGen 侧自然提供 **1×秘密多项式 \(s\)** + **1×噪声 \(e\)**；设备内复制 \(\hat{s}\) 以满足 \(K=4\) 等维度。

```text
host: src 逻辑 1s+1e；a_hat [K·K, N] 行主序（行 18）
设备:
  S1  limb6 → S0
  S2  AIC MMAD ×2 → mat_c 平面 [96,128]
  S3  平面 merge + mod q → dst（ŝ, ê_hat, …）
  行18  NTT域 basemul(Â, ŝ) + ê + mod → t_hat
  行19–20 ByteEncode₁₂ → ek / sk
```

### 1.2 与已冻结路线的差异（原理层）

| 维度 | 2s1e（活跃） | 已冻结 sepair / polybatch |
|------|-------------|---------------------------|
| mat_c | **平面**，bulk DataCopy | 竖堆 + Gather |
| 行 18 \(\hat{s}\) | **每 AIV 本地完整** | GM peer 交换 |
| host 布局 | 1s+1e | se_pair 交错 |

---

## 2. 向量/标量分工原则

### 2.1 默认规则

| 运算类 | 设备 | golden |
|--------|------|--------|
| 大批量 limb 编码、merge、mod | **向量** | 标量 C ref |
| 复杂控制流、位打包 | 向量或标量按探针 | 标量 |
| NTT 域 basemul（当前） | **标量**（热点） | 标量 |
| 语义不明的小循环 | 先标量对拍，再向量化 | 不变 |

**原则**：golden = **I/O oracle**；设备可用 Barrett/向量路径，**最终 int32 多项式一致即可**。

### 2.2 Stage1 数学要点

\[
\mathrm{lo} = v - (v \gg 6) \cdot 64
\]

**不是** `v & 63`（负数与 pipeline 语义不同）。向量化用 `ShiftRight` + `Muls` + `Sub`。

### 2.3 阶段表（当前默认）

| 阶段 | 实现 | 备注 |
|------|------|------|
| Stage1 | 向量 bulk（`F203_STAGE1_SPLIT=1`） | 禁止 `And(v,63)` |
| Stage2 | AIC MMAD | 与 Tag5T 同 |
| Stage3 | 向量 merge + Barrett | 无 Gather |
| 行 18 basemul | 标量 | 向量核见 alg11-12 探针 |
| 行 18 mod | Barrett 向量 | `F203_MOD_VARIANT=1` |
| ByteEncode | 向量 prefetch（`BYTE_ENCODE12_PREFETCH=1`）或 tile32 Gather+pack | NTT 禁令 **不** 覆盖；见 [F203-ByteEncode12-prefetch技术总结.md](F203-ByteEncode12-prefetch技术总结.md) |

---

## 3. Gather 禁令范围（易误解）

| 范围 | Gather |
|------|--------|
| **Tag5T NTT S1–S3** | **禁止** — 用平面 mat_c + bulk DataCopy |
| **NTT 之后**（basemul、ByteEncode 等） | **不自动禁止** — 单项验证 |

2s1e 当前 ByteEncode 用 Gather 是 **工程选型**，不是政策排斥 post-NTT Gather。

---

## 4. 模运算三档（设备）

| 变体 | 行为 | 用途 |
|------|------|------|
| 0 | int64 floor mod | golden |
| 1 | Barrett 向量 | **行 18 默认** |
| 2 | Cast 截断 | 对照 |

`HAT_GOLDEN_MOD_VARIANT=0` **固定**；改设备宏不改变 golden。

---

## 5. UB 融合不变量

单 `TPipe` 串联 S3 → 行 18 →（可选）ByteEncode：

1. \(\hat{s},\hat{e}\) **驻留 UB**；行 18 只读本地 ŝ  
2. \(\widehat{A}\) 从 GM 读；**S3 与行 18 之间不写 GM**  
3. TQue ≤8；中间量用 **TBuf** — [ascendc-TQue与Pipe框架知识库.md](ascendc-TQue与Pipe框架知识库.md)  
4. CPU 可用 `mixPass` 5→4 分段；SIM 生产路径 `mixPass=0`  

详见 [F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md)。

---

## 6. 验证方法论

| 做法 | 说明 |
|------|------|
| Stage Gate | 先 NTT only → 再内积 only → 再融合 |
| Early Probe | 每 Gate 后 head 系数抽检 |
| CPU 孪生 | 标量语义路径；勿强迫 CPU 跑完整 TQue 硬事件 |
| 性能 | SIM tick ≠ 墙钟；分段 tick 之和 ≠ 融合 tick |

---

## 7. 附录

### 7.1 验收命令

```bash
cd ascendc-tests/frozen/frozen-fix-f203-2s1e-alg13-16171820-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

### 7.2 永久避免（NTT 段）

| 能力 | 原因 |
|------|------|
| NTT S1–S3 Gather | 平面布局已替代 |
| `SHAT_PEER` | sepair 已冻结 |
| Stage1 `And(v,63)` | 语义错 |
| fork frozen alg13/tag5t 批 | 见 FROZEN.md |

### 7.3 向量 basemul 继任

[`pass-fix-f203-alg11-12-multiplyntts-k4`](../../ascendc-tests/pass-fix-f203-alg11-12-multiplyntts-k4/) — ROM + Init DataCopy；**勿** fork `frozen-fix-f203-2s1e-basemul-vec-k4`。

### 7.4 相关

- [F203-ByteEncode12-prefetch技术总结.md](F203-ByteEncode12-prefetch技术总结.md)
- [F203-innerproduct-k4-技术总结.md](F203-innerproduct-k4-技术总结.md)
- [qa/2026-06-19 ByteEncode prefetch](../../qa/2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md)
- [qa/2026-06-15 ByteEncode tile32](../../qa/2026-06/2026-06-15-ByteEncode12向量与Scatter讨论.md)

---

*2026-06-19：ByteEncode prefetch 交叉引用。2026-06-18：重构为原理优先。*
