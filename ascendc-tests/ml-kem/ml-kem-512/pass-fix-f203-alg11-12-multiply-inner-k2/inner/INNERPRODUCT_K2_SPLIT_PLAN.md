# ML-KEM-512 行 18 内积 k2 分片说明

## 目标

本目录验证 ML-KEM-512 `InnerProduct`：`P_OUT=2`、`S_VEC=2`、`N=256`。

```text
t_hat[p] = Σ_j MultiplyNTTs(a_hat[p,j], s_hat[j]) mod q
a_hat.bin : flat(p,j,c) = (p * S_VEC + j) * N + c
s_hat.bin : flat(j,c)   = j * N + c
```

## 双 AIV 分工

| AIV | 输出行 |
|-----|--------|
| 0 | `t_hat[0]` |
| 1 | `t_hat[1]` |

- `blockDim=2`，每个 AIV 恰好负责 1 行。
- 这是 k=2 锁定几何；禁止保留 k=3 的 `P_OUT/S_VEC=3`。
- 不使用临时 `P_OUT/2` 推导，避免后续迁移其它 k 时静默改义。

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
