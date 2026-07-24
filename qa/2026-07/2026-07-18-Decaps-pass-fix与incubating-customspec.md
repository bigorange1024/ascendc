# 2026-07-18 — Decaps pass-fix 更名 · incubating customspec

## 1. 结论

[`fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/) → [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)。

> **更名残留（2026-07-20 补记）**：`git mv` 只搬跟踪文件；旧路径下 `build_*`/`input`/`output` 曾留下**无源码空壳**。已删；**禁止再建**该旧名或误名 `pass-probe-*`。扫描/清理：[`scripts/cleanup-ascendc-test-ghosts.sh`](../../scripts/cleanup-ascendc-test-ghosts.sh)。

依据：全链+E3+T2 已绿；liboqs KAT CPU×10+SIM×3；device roundtrip CPU/SIM（含拒绝）；无 vendor；scripts 默认已指。`fo_only` 过渡与 `#交付#` **不**挡 `pass-fix` 更名（对齐 Encaps 先例）。

## 2. 同步

| 项 | 动作 |
|----|------|
| `ascendc-tests/INDEX.md` | device 迁入活跃 `pass-fix` 表 |
| `scripts/*decaps*` / `liboqs_kem_vs` / roundtrip | 默认 `DECAPS_DIR`→pass-fix |
| `AGENT_HANDOFF` / `AGENTS` / `README` | 下一刀改为 `#交付#` |
| NPU 说明 | 取消「跳过 Decaps device」 |

## 3. 当时未做 / 后续闭合

- 当日会话末曾写「未提交」；**2026-07-20** 已随 Decaps 交付链合入 `main`（`pass-fix` 更名 + incubating customspec 同批）
- `fo_only` 内联 → 打开项 **T19i**（仍开）
- `#交付#` stable → **2026-07-20 已完成**（见当日纪要）

## 4. incubating Decaps `$规格$`（ascendc-impl-spec）

新建 [`examples/incubating/exp-fips203-mlkem-kem-decaps-k4/`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/)：
仅 [`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex/.pdf`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.pdf)。

| 锁定项 | 约定 |
|--------|------|
| 基线 | `pass-fix-f203-alg21-kem-decaps-device-k4`（只读行为） |
| I/O | `dk_kem`(3168)+`c`(1568)+LUT → **仅** `K`(32) |
| Launch | SIM **4** / CPU **6**（含过渡 `fo_only`） |
| 自包含 | vendor Decrypt+Encrypt；单库；默认 `decaps_1session` |
| 本轮 | **无** kernel/main/CMake/run.sh；待确认后【预研】 |

已刷新：`examples/incubating/INDEX.md`、API 查阅索引、本纪要。

