# 自包含约束 — fix-f203-alg14-lines3-15-encrypt-prep-k4

对齐 [`docs/engineering/用例自包含与设备全链约束.md`](../../docs/engineering/用例自包含与设备全链约束.md)。

## 1. 允许的外部依赖

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`scripts/`、`fixtures/`、`cmake/` | `#include` / Python `import` **其它 ascendc-tests 用例** |
| 编译期 `#include` **`library/shared/`**（SHAKE/Keccak） | 运行时引用 `pass-fix-*` / `fix-f203-*`（本目录除外）路径 |
| 仓库级 `scripts/sim_env.sh`、`kernel-run-timeout.sh` | `gen_data.py` 从 `examples/stable/.../output` 动态拷贝 `ek` |
| 维护时手动 `scripts/vendor_sync_from_stable_keygen.sh`（**非**默认 `run.sh` 硬依赖） | |

## 2. Golden

- **唯一入口**：`scripts/gen_data.py`
- **原语**：`scripts/golden_encrypt_prep.py`（Alg.7 + PRF/CBD 已抄写固化）
- **几何常量**：`scripts/prep/alg7_geom.py`（与 vendored `prep/alg7/f203_alg7_layout.h` 同步）
- **不** `sys.path` 到 `library/shared` 或其它探针

## 3. 输入 ek_pke

- **权威副本**：`fixtures/ek_pke.bin`（1568B，见 `fixtures/README.md`）
- 每次 `run.sh`：`gen_data.py` 复制到 `input/ek_pke.bin`（`input/` 为运行态，可 gitignore）

## 4. 审查

```bash
cd ascendc-tests/fix-f203-alg14-lines3-15-encrypt-prep-k4
rg 'pass-fix-f203|fix-f203-alg(?!14-lines3-15-encrypt-prep)' scripts/gen_data.py scripts/golden_encrypt_prep.py
rg 'examples/stable|library/shared/fips203' scripts/gen_data.py scripts/golden_encrypt_prep.py
rg '#include.*ascendc-tests/(pass|fix)-' . --glob '*.{cpp,hpp}'
```

（`prep/` vendored 注释中的历史探针名不计入运行依赖。）
