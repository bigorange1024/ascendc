# 源码注释说明 — pass-fix-f203-alg13-device-keygen-k4

## `@probe` 文件头

每个 `.cpp` / `.h` / `.hpp` / `.cmake` / `.sh` / `.py`（排除 `build*`、`out*`、`sim_log*` 等）顶部含：

```
@probe pass-fix-f203-alg13-device-keygen-k4
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

1. `pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex`
2. `run.sh` → `main_keygen.cpp`
3. `f203_keygen_prep_entry.cpp` → `f203_keygen_prep_ub.hpp`
4. `compute/mmad_custom.cpp`

## 实现方案 PDF

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-device-keygen-k4
bash ../../scripts/xelatex-clean.sh pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex
```
