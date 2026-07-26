# examples/incubating — 研究中算子

**自包含**（2026-06-29）：与探针同约束，见 [用例自包含与设备全链约束.md](../../docs/engineering/用例自包含与设备全链约束.md)。

**前缀**：`exp-<简述>/`。

**规则**（见 Rule）：研究类代码**只能先写此处**；定型后**复制**到 `examples/stable/…`，本目录副本**保留**。

**废弃实验** → [../frozen/INDEX.md](../frozen/INDEX.md)（`frozen-exp-*` — **路线关闭，禁止抄码、禁止用其 customspec**）。

---

## 目录结构（2026-07-26）

```text
examples/incubating/
├── ml-kem/
│   └── ml-kem-1024/     # FIPS 203 ML-KEM-1024（k=4）活跃 exp-*
│       └── INDEX.md
├── frozen/              # 见 ../frozen/（不在本 INDEX 树内展开）
└── INDEX.md
```

| 路径 | 角色 |
|------|------|
| [ml-kem/](ml-kem/INDEX.md) | ML-KEM 预研按参数组 |
| [ml-kem/ml-kem-1024/](ml-kem/ml-kem-1024/INDEX.md) | **当前** ML-KEM-1024 `exp-*` 详表 |

---

## 维护

新增 `exp-*` → 写入对应参数组目录并更新该组 `INDEX.md`；晋级 stable 后**不删除**预研行（可标「已复制至 stable-…」）。
