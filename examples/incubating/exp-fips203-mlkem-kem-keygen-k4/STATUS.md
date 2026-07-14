# STATUS — exp-fips203-mlkem-kem-keygen-k4

FIPS 203 **Algorithm 19 `ML-KEM.KeyGen()`**（ml_kem_1024 / k=4）— incubating **【预研】** 重写（2026-07-14）。

| 项 | 值 |
|----|-----|
| **状态** | **有条件完成**（incubating CPU×40 + SIM + KAT；**未** `#交付#` / 未建 stable） |
| **customspec** | [`exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf`](exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf) |
| **registry** | [`docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md) |
| **Launch** | **2**（prep ‖ mmad+KemKgTailFused） |
| **I/O** | `seed_d`+LUT → `ek_kem` 1568B / `dk_kem` 3168B |
| **SIM tick** | **707057**（`SIM_DIRECT=1`，SEED_D=20260619） |

## 踩坑落地（相对 2026-07-13 办公室失败）

| 条款 | 本树实现 |
|------|----------|
| SIM/设备双 AIV 汇合 | Encode 后 `SyncAll<isAIVOnly=true>()`，再 **AIV0** Fuse/Tail |
| CPU 禁 SyncAll 死等 | **AIV1** Fuse/Tail + `dk_kem_gm[0:2]` Encode-done **软旗**（防偶发并行抢跑 poly） |
| 禁空 `KYBER_PIPE_ALL` | `kyber_limb6.hpp` 恒 `PipeBarrier<PIPE_ALL>` |
| 禁残留侥幸 | `run.sh` VERIFY 前清零 `output/*.bin`；host 清零 `sk`/`dk_kem` |

## 验收证据（2026-07-14）

| 门禁 | 命令 / 结果 |
|------|-------------|
| CPU 单次 | `bash run.sh -r cpu -v Ascend910B4` → ek/dk max=0 |
| CPU 压测 | 清零 output **40/40 PASS** |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim` → max=0；tick **707057**；根目录无 stray dump |
| vs correctness | `SEED_D=20260619..28` ×10，`cmp` ek/dk **一致** |
| vs liboqs | CPU×10 PASS；**SIM×3 PASS**（`KEM_KG_SIM_TRIALS=3 SIM_DIRECT=1`，随机 `kem_seed`；log `output/liboqs_kem_keygen/kat_sim3_20260714.log`） |

## 未做 / 禁止

- **未**复制晋级 `examples/stable/`（须用户 `#交付#`）
- 禁止编译期依赖 `pass-fix-…-device-k4`（仅对照行为）；本树 **vendored** PKE + `kem/`

## 下一步

用户确认稳定后 `#交付#` → `stable-fips203-mlkem-kem-keygen-k4`；并行遗留 T19a Encaps device。
