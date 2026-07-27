# INTEGRATION_PLAN — pass-fix-f203-alg13-device-keygen-k2

**技术总结**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](../../../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)

**适配来源（活跃）**：[`ml-kem-768/pass-fix-f203-alg13-device-keygen-k3`](../../ml-kem-768/pass-fix-f203-alg13-device-keygen-k3/)

**讨论**：[`qa/TODO.md`](../../../../qa/TODO.md) T512

## 1. 目标

| 项 | 锁定值 |
|----|--------|
| 参数组 | ML-KEM-512，`k=2` |
| 生产 I/O / launch | 2 launch，`seed_d`+LUT → `ek_pke=800B`、`dk_pke=768B` |
| prep Â | 独立 prep Â `[4,256]`，双 AIV **2+2** |
| `s‖e` | true polyvec4 `[4,256]`，Alg.13 中 s/e 均用 CBD-η3 |
| compute | S-1 `MIX blockDim=1`，polyvec4 NTT，平面 `mat_c[32,128]`，InnerProduct **1+1** |

## 2. 唯一代码差分（首轮）

- 从活跃 k3 D13 复制后，收缩所有 `k=3` 几何到 `k=2`：Â `9→4`、`src 6→4`、`S0 12→8`、`mat_c 48→32`、`dst 6→4`。
- CBD 采样切到 `f203_cbd_eta3` / `sample_poly_cbd3`，并锁定 derand prefix `exp-mlkem-f203-2s1e-k2:SEED_D=`。
- ByteEncode12 仅打包 `t_hat[2,256]`：`2×384=768`，再追加 `ρ[32]` 得 `ek_pke=800B`。

## 3. 验收

与用户验收一致：

```bash
KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
KEYGEN_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

本轮不建 `examples/`，不建 stable-512。

## 4. 自包含与设备全链

见 [`SELF_CONTAINED.md`](SELF_CONTAINED.md)：

- 源码仅本目录 + `library/shared`（编译期）；禁止引用其它探针/example 树
- 默认 `run.sh`：**设备** 2 launch 完成 Alg.13；Host 仅 seed+静态 LUT；`KEYGEN_VERIFY=1` 为 oracle 对拍
