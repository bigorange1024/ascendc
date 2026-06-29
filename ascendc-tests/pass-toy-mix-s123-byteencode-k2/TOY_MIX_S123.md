# pass-toy-mix-s123-byteencode-k2 — MIX 三阶段 + 双 AIV 玩具探针

## 目的

在 **不含跨 AIV ŝ 交换** 的前提下，用最小数字验证：

1. **Stage1（AIV×2）**：limb 玩具 + 填数 → `ws+S0` 左矩阵 `A[int8,64×64]`
2. **Stage2（AIC）**：Cube `64×64×64` 矩阵乘，`B=I₆₄` → `C[int32,64×64]`
3. **Stage3+encode（AIV×2）**：各取 `C` 一半 → UB → `Adds(+1)` → `func1(%64→int8)` → 写 `out`

单趟 PEM；**AIC↔AIV** 用 `CrossCore SET/WAIT`；**无 AIV↔AIV** 同步。

## 维度与分片（方案 B）

| 符号 | 形状 / 大小 | 说明 |
|------|-------------|------|
| `src` | `2048 × int32` | Python 输入，初值全 0 |
| 每 AIV `src` 片 | `1024 × int32` | AIV0 `[0:1024]`，AIV1 `[1024:2048]` |
| 玩具 limb | `1024 int32 → 2048 int8` | 每系数拆 `lo6`、`hi6` 各 1 字节（非真 limb6） |
| `S0` / `A` | `4096 int8` = `64×64` | AIV0 写 flat `[0:2048]`，AIV1 写 `[2048:4096]` |
| 填数规则 | `A[i] = i % 128` | `i` 为 flat 下标，行优先 |
| `LUT` / `B` | `64×64 int8` | 单位阵 `I₆₄` |
| `MAT_C` / `C` | `64×64 int32` | `C = A @ B`，`B=I` 故 `C=A`（int32） |
| 每 AIV `C` 片 | `2048 int32` | AIV0 `[0:2048]`，AIV1 `[2048:4096]` |
| `out` | `4096 int8` | 每 AIV `2048`；`out[i] = (C[i]+1) % 64` |

## Golden（Python）

```python
A = np.arange(4096, dtype=np.int32) % 128   # 与设备填数一致
A8 = A.astype(np.int8)                    # 左矩阵
B = np.eye(64, dtype=np.int8)             # 单位阵
C = (A8.astype(np.int32) @ B.astype(np.int32)).reshape(-1)  # == A
out = ((C + 1) % 64).astype(np.int8)
```

## Workspace 布局

```
S0      : 4096 B   int8  左矩阵 A
LUT     : 4096 B   int8  右矩阵 B
MAT_C   : 16384 B  int32 Cube 输出 C
```

## MIX 同步（MachineState）

| Flag | 值 | 含义 |
|------|---|------|
| `AIV_SPLIT` | 1 | AIV Stage1 完成 |
| `AIC_MMAD` | 2 | AIC matmul 完成 |

流程：

```
AIV: S1 → SET(AIV_SPLIT)
AIC: WAIT(AIV_SPLIT) → mmad 64³ → SET(AIC_MMAD)
AIV: WAIT(AIC_MMAD) → DataCopy 半片 C 到 UB → Adds(+1) → func1 → CopyOut out
```

## UB 驻留约定

- Stage3 **入口**允许一次 `DataCopy`：`MAT_C` → UB
- **禁止** Stage3 内 Adds/func1 之间把中间结果写回 GM
- `func1` 在 UB 内嵌 C 标量循环
- 最终 `out` 写 GM 为程序输出

## mixPass

| mixPass | 内容 |
|---------|------|
| 0 | S1+S2+S3+func1 全流程 |
| 1 | 仅 S1 |
| 2 | 仅 S2（需 `s0_preset`） |
| 3 | 仅 S3+func1（需 `mat_c_preset`） |

## 与真 NTT / keygen 的对应

| 本 toy | 真流程 |
|--------|--------|
| 玩具 limb + 填 0..127 | `AivSplitPolyBatch` |
| `AicMmad(64,64,64)` | `AicMmad(16,256,128)` ×4 |
| `Adds(+1)` | Stage3 向量 + 模加 |
| `func1 %64` | `ByteEncode₁₂` |
| （未实现） | 行 18 + 跨 AIV ŝ 交换 |

## 验收

```bash
cd ascendc-tests/pass-toy-mix-s123-byteencode-k2
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

`verify_result.py`：`out.bin` 与 golden 逐元素 `max_abs_diff=0`。
