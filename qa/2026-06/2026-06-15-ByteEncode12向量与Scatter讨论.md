# 2026-06-15 — ByteEncode₁₂ 向量化、Scatter 与 2s1e 剩余热点

**记录时间**：2026-06-15

**探针**：[`pass-fix-f203-2s1e-byteencode12-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-byteencode12-vec-k4/)（自 `fix-f203-2s1e-alg13-16171820-k4` fork，仅改 ByteEncode）

---

## 结论摘要

1. **ByteEncode₁₂ 向量主路径已落地**：Gather 解交错 + `int32` 向量位运算（`ShiftRight`/`Muls`/`Sub`/`Add`）→ `b0W/b1W/b2W`；**勿**经 `int8` Cast（128–255 饱和致 SIM 失败）。
2. **`AscendC::Scatter` 在 Ascend910B4 不可用**：`NpuArch=2201`（`dav_c220`）下 `ScatterImpl` 为 `NOT_SUPPORT`；CPU sim 亦不支持 `uint8` Scatter dtype。CANN scatter 样例亦说明 910B 需**改算法**兼容。
3. **交织写拍板**：`BYTE_ENCODE12_SCATTER_VEC=1` → **4 pair 打成 3×`int32`（12B）+ `DataCopy` 96B/tile**，非 Scatter API。CPU + SIM 全链路 `max_abs_diff=0`。
4. **SIM 全链路墙钟（mixPass=0）**：标量交织 **~53.9s** → pack+DataCopy **~47.5s**（约 **12%**）；encode 仅占全链路一小段。
5. **全链路剩余算力热点**：行 18 **`multiply_ntts_half_scalar`（basemul）**；累加、`+ê`、final mod 已向量化。`multiply_ntts_half_vec`/`coef_pairs_vec` 仍冻结（勿抄 frozen）。
6. **ByteEncode 推广**：`d∈{1,4,5,10,11,12}` 需 `EncodeTraits<d>` 抽象，非仅 `d=12`；`uint16 And` 路径待 CPU sim Cast 能力验证后再开。

---

## 1. 探针与宏

| 宏 | 默认 | 含义 |
|----|------|------|
| `BYTE_ENCODE12_VEC` | `1` | `0`=`poly_byte_encode12_scalar`；`1`=Gather + 向量位运算 |
| `BYTE_ENCODE12_SCATTER_VEC` | `1` | `0`=标量 `SetValue` 交织；`1`=pack_quad12 + `DataCopy` |

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-byteencode12-vec-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
# 对照标量交织：
BYTE_ENCODE12_SCATTER_VEC=0 bash run.sh -r sim -v Ascend910B4
```

方案细节：[BYTE_ENCODE12_VEC.md](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-byteencode12-vec-k4/BYTE_ENCODE12_VEC.md)

---

## 2. ByteEncode 向量流水线（tile=32 pair）

```text
a[256] int32
  │ DataCopy 64 系数/tile
  ▼
Gather×2（字节偏移 8i / 8i+4）→ t0[32], t1[32]
  │ int32 mask/shift（12-bit）
  ▼
b0W, b1W, b2W（int32 工作区，值域 0..255）
  │ BYTE_ENCODE12_SCATTER_VEC=1
  ▼
pack_quad12_i32（8 组×4 pair → 24×int32）
  │ DataCopy 96B → r[byteBase]
  ▼
r[384] → GM ek/sk
```

**Gather**：post-NTT 步骤，**不受 NTT S1–S3 Gather 禁令**。

**交织打包（小端，与 FIPS `[b0,b1,b2,...]` 一致）**：

```text
pair 0..3 @ bytes 0..11:
  w0 = [b0₀,b1₀,b2₀,b0₁]
  w1 = [b1₁,b2₁,b0₂,b1₂]
  w2 = [b2₂,b0₃,b1₃,b2₃]
```

---

## 3. Scatter API 调研结论（910B4）

| 尝试 | 结果 |
|------|------|
| `Scatter(uint8)` | CPU sim dtype check 失败 |
| `Scatter(int32)` + 字节偏移 0,3,6,… | 每次写 4B，破坏 stride-3 布局 |
| `Scatter(int32)` + 4-pair 打包 | dtype 通过，但 **2201 ScatterImpl 仍 NOT_SUPPORT** |
| **pack + `DataCopy`** | **可用**；与 910B 无 Scatter 硬件一致 |

**芯片支持 Scatter 的 arch**（参考 CANN 头文件）：3002/3102/3510/5102 等；**910B4=2201 不在列**。

后续若上 910C 等，可再评估真 `uint8` Scatter×3；当前交付以 pack+DataCopy 为准。

---

## 4. 踩坑记录

| 问题 | 原因 | 修复 |
|------|------|------|
| SIM ek/sk 大面积错 | `int8` 存 128–255 饱和为 127 | scatter 从 `int32 & 0xFF`；pack 用 `uint8_t` 截断 |
| CPU `And`/`half→int32` Cast | tikicpulib 能力缺失 | 全程 `int32` Shift/Muls/Sub |
| `b1` 覆盖 `b2` | 复用 tmp 时序 | `b2` 先入 `b2W` 再算 `b1` |
| Scatter 路径 SIGABRT | 2201 未实现 Scatter | 改 DataCopy |

---

## 5. 2s1e 全链路：谁还向量化、谁仍标量

| 阶段 | 实现 | 备注 |
|------|------|------|
| Stage1 | 向量（`F203_STAGE1_SPLIT=1`） | bulk Shift/Cast |
| Stage2 MMAD | Cube | — |
| Stage3 merge+mod | 向量 Barrett | 平面 mat_c |
| 行 18 basemul | **标量** `multiply_ntts_half_scalar` | **主瓶颈** |
| 行 18 ∑ / `+ê` / mod | 向量 `Add` + `MOD_Q_*` | — |
| 行 19–20 ByteEncode | **向量**（本探针） | pack 循环仍小标量 |

**下一优化方向**：**不用 Gather 的向量 basemul**（`f/g` 已交错 `[a0,a1,...]`，直接向量 `Mul` + Barrett reduce）；与 ByteEncode 无关。

---

## 6. 与主线关系

- 本探针验证通过后，可合并 ByteEncode 向量实现回 [`fix-f203-2s1e-alg13-16171820-k4`](../../ascendc-tests/frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)。
- 主线 NTT + 行 18 逻辑不变；仅 `byte_encode12_*` 与 UB scratch（+~1.6KB）迁入。
- [`pass-toy-mix-s123-byteencode-k2`](../../ascendc-tests/pass-toy-mix-s123-byteencode-k2/) 为 MIX 玩具，非 Alg.13 全链路。

---

## 7. 索引与备份

- 探针 STATUS：[STATUS.md](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-byteencode12-vec-k4/STATUS.md)
- 实现指南 §3 / §7.5 已刷新：[MLKEM-NTT-向量与标量实现指南.md](../../docs/notes/MLKEM-NTT-向量与标量实现指南.md)
- 当日备份：`backup/v0.1_20260615193314/`（`bash backup-project.sh`）

---

## 8. 行 18 basemul spike（同日晚间，`fix-f203-2s1e-basemul-vec-k4` → **2026-06-16 冻结**）

> ⛔ 已迁入 [`frozen/frozen-fix-f203-2s1e-basemul-vec-k4`](../../ascendc-tests/frozen/frozen-fix-f203-2s1e-basemul-vec-k4/)。继任：[`pass-fix-f203-alg11-12-multiplyntts-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-multiplyntts-k4/)。

| 变体 | CPU | SIM | 说明 |
|------|-----|-----|------|
| 0 标量 | ✓ | ✓ | 基线 |
| 1 deinterleave+vec | ✓ | ✓ | 纯向量 Barrett **SIM 失败**；标量 `hat_reduce_zq_scalar` 收尾后通过 |
| 2 gather+vec | ✓ | ✓ | post-NTT Gather 可用 |

SIM 全链路：标量 ~50s；变体 1 ~80s、2 ~69s（**更慢**）。详见 [BASEMUL_VEC.md](../../ascendc-tests/frozen/frozen-fix-f203-2s1e-basemul-vec-k4/BASEMUL_VEC.md)、[FROZEN.md](../../ascendc-tests/frozen/frozen-fix-f203-2s1e-basemul-vec-k4/FROZEN.md)。
