# 自包含与设备全链约束 — stable-fips203-mlkem-kem-decaps-k4

customspec：[`stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex`](stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex)

**定型交付**（2026-07-20 `#交付#`）：自 `examples/incubating/exp-fips203-mlkem-kem-decaps-k4` **复制**晋级；预研副本保留。

与仓库 [`docs/engineering/用例自包含与设备全链约束.md`](../../../docs/engineering/用例自包含与设备全链约束.md) 对齐：

| 项 | 本目录 |
|----|--------|
| AscendC / Host 源码 | **全部 vendored**（Decrypt + Encrypt + kem） |
| 允许的外部依赖 | `library/shared/`；Host golden 可读 Encrypt `scripts/host_golden` / `thirdparty`（软链，非编译期 `#include`） |
| 禁止 | 编译期长期依赖 `ascendc-tests/`、其它 `examples/`、`frozen/` |
| 生产 I/O | `dk_kem`+`c`+LUT → **仅** `K` |

行为基线（只读）：[`pass-fix-f203-alg21-kem-decaps-device-k4`](../../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)。
