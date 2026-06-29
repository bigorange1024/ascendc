# examples — 算子计算说明索引

**本 INDEX 的重点**：说明 `examples/` 下**每个子目录在算什么**（数学语义、数据类型与规模、工程角色）。运行方式见各目录 `RUN.md` 或 `run.sh`。

---

## 分层

| 路径 | 角色 |
|------|------|
| [incubating/](incubating/INDEX.md) | 研究中：`exp-<简述>/`，预研代码**只写这里** |
| [stable/](stable/INDEX.md) | 定型：`stable-<简述>-vN/`，从 `exp-*` **复制**晋级 |
| [frozen/](frozen/INDEX.md) | **路线关闭**：`frozen-exp-*`；只读 `FROZEN.md`/INDEX；**禁止抄码、禁止用 frozen customspec** |
| [../ascendc-tests/](../ascendc-tests/INDEX.md) | 平台功能探针；已关闭路线见 `ascendc-tests/frozen/`（同上） |

---

## 子目录与计算内容

### `incubating/`、`stable/`

见 [incubating/INDEX.md](incubating/INDEX.md)、[stable/INDEX.md](stable/INDEX.md)。

---

## 维护

新增 `exp-*` 或 `stable-*` → 更新 `incubating/` 或 `stable/` 的 `INDEX.md`，并在本文件增加一节（计算一句话、dtype、规模、角色）。
