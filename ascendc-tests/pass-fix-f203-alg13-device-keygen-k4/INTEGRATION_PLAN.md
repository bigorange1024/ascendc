# INTEGRATION_PLAN — pass-fix-f203-alg13-device-keygen-k4

**技术总结**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)

**语义基线（只读）**：[`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/)

**讨论**：[`qa/TODO.md`](../../qa/TODO.md) T13h

## 1. 目标

| 项 | pass 探针 | 本探针 |
|----|-----------|--------|
| prep Â | block0 串行 shard  subprocess1 | **GetBlockIdx() 并行** 8+8 |
| 生产 I/O / launch | 2 launch，seed+LUT→ek/dk | **相同** |
| 预期 prep SIM tick | ~801k | ~380–450k（参照 [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../pass-fix-f203-alg13-lines3-7-a-hat-k4/) ~381k） |

## 2. 唯一代码差分（首轮）

- `f203_keygen_prep_ub.hpp`：`BuildAHat16ShardWithUb(..., blockIdx, ...)` 双 AIV 并行（去掉 `#if` 串行分支）

后续若 SIM 仍 FAIL，按 `PIPE_SYNC_EVAL.md` 修 P-02/P-04 或对齐 `prep/ahat/f203_a_hat16_ub.hpp` 与独立 a_hat 探针。

## 3. 验收

与 pass 同级：`bash run.sh` CPU+SIM；`KEYGEN_VERIFY=1`；可选 `kat_liboqs_vs_ascendc.sh`。

**晋级**：本探针 CPU+SIM+KAT 全过后，再同步 `exp-mlkem-f203-pke-keygen-k4`；pass 探针保持不动直至用户确认继任。

## 4. 自包含与设备全链

见 [`SELF_CONTAINED.md`](SELF_CONTAINED.md)：

- 源码仅本目录 + `library/shared`（编译期）；禁止引用其它探针/example 树
- 默认 `run.sh`：**设备** 2 launch 完成 Alg.13；Host 仅 seed+静态 LUT；`KEYGEN_VERIFY=1` 为 oracle 对拍
