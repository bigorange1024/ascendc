# frozen-exp-mlkem-f203-stage12-encode-matmul-mix — **路线废弃中止**

> ⛔ **已冻结** — 见 [FROZEN.md](FROZEN.md)。**禁止抄码、禁止用本目录 customspec。**

| 字段 | 内容 |
|------|------|
| **状态** | **废弃冻结**（2026-06-10 标废弃，2026-06-11 迁入 `examples/frozen/`） |
| **原因** | LeakyRelu 融合 MIX + `Matmul<>`；CrossCore 不透明；sim encode 写 GM 失败 |
| **继任** | `exp-sepolyvec8-ntt-k8/`；F203：`fix-f203-2s1e-alg13-16171820-k4` |

## 勿再使用

- 勿 fork 作 MIX 三段壳。
- 勿以 `*-customspec.pdf` 作实现依据。

见 [examples/frozen/INDEX.md](../INDEX.md)。
