# 自包含与设备全链约束 — pass-fix-f203-alg19-kem-keygen-device-k4

## 密码学契约

Alg.19 `d`/`z` device UB、3168B `dk_kem` liboqs 布局、Host 仅 `seed_d` + LUT。历史 correctness oracle **已冻结**（只读 [`FROZEN.md`](../frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/FROZEN.md)；禁翻源码）。

## 工程差异（相对 correctness）

| 项 | correctness | **device-k4** |
|----|-------------|---------------|
| PKE 源码 | `vendor/pke_keygen` rsync | **编译期引用** [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/)（见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §4） |
| Launch | 3（独立 `kem_finish`） | **2**（Alg.16 尾内嵌 `mmad`） |
| `ek_kem` | 独立 GM + 拷贝 | **`ek_pke_gm` 别名，零拷贝** |

**阶段性说明（2026-07-10）**：Phase 1 允许编译期依赖 stable 路径；若日后要求单目录自包含，仅将 stable 树 copy 进本目录，**不改变** 2 launch / GM 契约。

## P1 工程定案（2026-07-10 保留）

| 项 | 做法 | 保留原因 |
|----|------|----------|
| mmad | stable `F203_KEM_KEYGEN_TAIL=1`，无 fork | 避免 stable 演进时 merge `mmad_custom_kem.cpp` |
| ROM | `scripts/prep/` → 本探针 `prep/alg7/` | 构建不改写 stable 树 |
| tick | ~713k（较 fork +~1.8%） | I/O 与 correctness 一致；工程收益 > tick 差 |

SHA3：尾段两处 SHA3-256 在 `kem/*.hpp`，日后可换第三方 AscendC 实现（见 INTEGRATION_PLAN §4.5）。

## 禁止

- 子进程调其它探针 `run.sh`
- 生产路径 Host 算 `H(ek)` / `z`
- 为 KEM 尾段增加第 3 launch 或 D2H→H2D 往返
- 从 `frozen/` 或 correctness `vendor/` **抄码**（可读 INTEGRATION_PLAN / STATUS 对照）
