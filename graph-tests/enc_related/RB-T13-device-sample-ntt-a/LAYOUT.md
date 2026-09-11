# LAYOUT — RB-T13 设备 Â←SampleNTT(ρ)

## 数据流（一句话）

Host ρ[32]（T07 ek 尾语义）→ **设备** 16×Alg.7 SampleNTT → Â[16,256]。

## 与 T07 / T12 / lines3-7 关系

| 刀/探针 | 角色 |
|---------|------|
| T07 | Host prep：ρ←ek 尾；本刀复用其 **ρ 字节语义**（FIXED_RHO） |
| T12 | MIX 半链壳（flag 1/3 + 极轻 Cube）；本刀同构换载荷 |
| lines3-7 | Â I/O 契约与 `BuildAHat16ShardWithUb` 积木（CMake `-I`，不抄源） |

## Flag

| flagId | 含义 |
|--------|------|
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7**（本刀无 GATE） |

## Â 布局

- `a_hat[16,256] int32` 行主序：`offset=(p*4+j)*256`，`p,j∈[0,4)`
- seed=`ρ‖byte(j)‖byte(i=p)`；XOF 固定 672B

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 SampleNTT 握手 |
| 有 POST_WAIT3 无 AHAT_DONE | 卡 SampleNTT / 写出 |
| AHAT 有但 max≠0 | ρ 字节 / XOF / rej 契约差 |
