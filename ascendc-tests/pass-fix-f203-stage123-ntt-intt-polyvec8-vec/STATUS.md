# STATUS — pass-fix-f203-stage123-ntt-intt-polyvec8-vec

**阶段**：终态（8-poly 三段式 NTT/INTT，CPU+SIM PASS）  
**构图**：Tag5T 单图 S1→S2→S3；**仅换 LUT** 切换 NTT/INTT（`kMlkemLimb6Ntt_T_i8` / `kMlkemLimb6Intt_T_i8`）

## 目标

- 输入 **8×256** polyvec（系数域 `int32`；与 ntt_study `sepolyvec8` 同口径）
- **紧凑 Stage1**：S0 布局 `[HI₈, LO₈]`（16 行，无插零）
- MIX：**1×AIC + 2×AIV**（`blockDim=1`；**1 launch** 跑完全链）

## 性能（910B4，mixPass=3 全链）

| 模式 | CPU | SIM totalTick |
|------|-----|---------------|
| NTT  | PASS | **30347** |
| INTT | PASS | **30340** |

## 正确性

- 设备 `dst` vs Python golden：**max=0**
- vs ntt_study C（Tag5T / F203 Tag3）：**max=0**（`scripts/cross_check_ntt_study_c.py --regen`）
- vs `sepolyvec8_ntt_f203` 交付 golden：**max=0**

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_NTT_MODE` | `ntt` | `ntt` 或 `intt`（切换 LUT） |
| `STAGE123_POLYVEC8_MIX_PASS` | `3` | 0=S1 / 1=S2 / 2=S3 / 3=全链 |

## 用法

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
F203_NTT_MODE=intt bash run.sh -r cpu -v Ascend910B4
F203_NTT_MODE=intt bash run.sh -r sim -v Ascend910B4
```

C 参考对拍（独立，不接入 run.sh）：

```bash
python3 scripts/cross_check_ntt_study_c.py --regen
```

## 参考

- 代码基线（fork 自）：[`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)
- ntt_study：`thirdparty/ntt_study/examples/mlkem/deliverables/sepolyvec8_ntt_f203/`
- 定稿 note：[F203-polyvec8-stage123-NTT-INTT技术总结.md](../../docs/notes/F203-polyvec8-stage123-NTT-INTT技术总结.md)
