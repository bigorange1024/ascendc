# 自包含与设备全链约束 — exp-fips203-mlkem-kem-decaps-k3

本目录为 **Alg.21 ML-KEM-768 KEM Decaps** incubating 实现。Phase-D 来自本目录 vendored E15/D15 Decrypt，Phase-E 来自本目录 vendored E14/D14 Encrypt，KEM 增量在 `kem/`。

## 1. 自包含

| 允许 | 禁止 |
|------|------|
| 本目录 vendored 源码：`compute/`、`prep/`、`unpack/`、`multiply/`、`kem/`、`scripts/host_golden/` | 编译期 `#include` 或运行时软链到 `ascendc-tests/` / 其它 `examples/` |
| `library/shared/` | 从 `**/frozen/**` 复制源码、customspec 或路线 |
| 本目录 `scripts/keygen_golden.py` 与其 `scripts/prep|compute` oracle 依赖 | 默认 Host 预填 `r'`、Host FO、Host memcmp 冒充生产 |

## 2. 设备全链

```text
input/  dk_kem.bin(2400) + c.bin(1088) + LUT
   → Phase-D Decrypt → m'
   → Phase-E G(m'‖h) + Encrypt(ek,m',r') + FO
output/ K.bin(32)
```

Host **禁止**在默认路径计算最终 K；`scripts/gen_data.py` 只生成合法/拒绝 oracle 和 LUT。

## 3. Golden

| 层级 | 用途 |
|------|------|
| `scripts/gen_data.py` | 默认全链 accept/reject input + golden K |
| `scripts/gen_data_phase_e.py` | 调试 Phase-E-only fixture（非默认） |
| `scripts/verify_kem_decaps.py` | 对拍 K；reject 时额外核验 `K=J(z‖c)` |
