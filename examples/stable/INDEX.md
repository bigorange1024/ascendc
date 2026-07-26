# examples/stable — 定型算子

**前缀**：`stable-<简述>/` 或 `stable-<简述>-vN`。

**规则**（见 Rule）：首版须从 `exp-*` **复制**晋级；修订须**新版本目录**，旧版标 **已取代**。

---

## 目录结构（2026-07-26）

```text
examples/stable/
├── ml-kem/
│   └── ml-kem-1024/     # FIPS 203 ML-KEM-1024（k=4）定型 stable-*
│       └── INDEX.md
└── INDEX.md
```

| 路径 | 角色 |
|------|------|
| [ml-kem/](ml-kem/INDEX.md) | ML-KEM 定型按参数组 |
| [ml-kem/ml-kem-1024/](ml-kem/ml-kem-1024/INDEX.md) | **当前** ML-KEM-1024 `stable-*` 详表（PKE+KEM 六算子 + Decaps CT 副本） |

---

## 维护

新增或取代版本 → 更新对应参数组 `INDEX.md` 与 [`../INDEX.md`](../INDEX.md)。
