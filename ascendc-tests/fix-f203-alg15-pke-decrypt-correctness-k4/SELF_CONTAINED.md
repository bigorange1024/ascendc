# 自包含与设备全链约束 — fix-f203-alg15-pke-decrypt-correctness-k4

对齐 Encrypt / KeyGen 探针教训。

## 1. 自包含

| 允许 | 禁止 |
|------|------|
| 本目录 vendored 源码 | 运行时 `#include` 其它探针路径 |
| `library/shared/` | liboqs / oqs.h |

## 2. 设备全链

```text
input/  dk_pke.bin + c.bin + LUT
   → device Alg.15 各段
output/ m.bin
```

Host **禁止**在默认路径做 decompress / NTT / 内积 / 提 m。

## 3. Golden

| 层级 | 用途 |
|------|------|
| `gate_g*.py` | 分阶段 |
| `golden_m.py` | `DECRYPT_VERIFY=1` |
