# 源码注释说明 — exp-fips203-mlkem-pke-keygen-k2

## `@probe` 文件头

每个 `.cpp` / `.h` / `.hpp` / `.cmake` / `.sh` / `.py`（排除 `build*`、`out*`、`sim_log*` 等）顶部含：

```
@probe exp-fips203-mlkem-pke-keygen-k2
@file   相对路径
@layer  host | prep | compute | cmake | script | legacy
@role   本文件职责（中英）
@production_io  默认 run.sh 的 I/O 契约
@launch blockDim / 核类型（host 脚本为 N/A）
@ai_core  SIM profile 要点；CPU SUCCESS 勿当真机依据
@depends  主要 #include 或调用关系
@verify   如何验收本文件相关路径
```

## 重新生成

```bash
python3 scripts/inject_probe_code_comments.py
```

已含 `@probe` 标记的文件会跳过。需强制重写：删除该行后重跑。

## 阅读顺序（新人）

1. `STATUS.md` / `INTEGRATION_PLAN.md`
2. `run.sh` → `main_keygen.cpp`
3. `f203_keygen_prep_entry.cpp` → `f203_keygen_prep_ub.hpp`
4. `compute/mmad_custom.cpp`

## 说明

本目录是 `ascendc-tests/` 探针，不提供 `examples/` customspec/PDF；实现锁定参数与验收证据以 `STATUS.md` 为准。
