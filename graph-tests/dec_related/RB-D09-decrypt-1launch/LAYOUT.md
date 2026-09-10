# LAYOUT — RB-D09-decrypt-1launch

## Host（1 launch）

```
Load dk_pke[1536], c[1568], zetas, gammas, mat_a, mat_b → ws
    │
    ▼
ACLRT_LAUNCH(d09_decrypt_fused_custom)   ← 唯一设备核
    │
    ▼
aclrtSynchronizeStream
    │
    ▼
D2H：m[32] · TRACE · out magic 0x44303931
```

## Device（同核串行）

| 段 | 角色 | 动作 |
|----|------|------|
| 0 | AIV0 | PrepUnpack：dk/c→ŝ/u/v（直读 H2D；UB+DataCopy 写出）+ TRACE PREP |
| 0b | ALL | `SyncAll()`（**Wait 环外**） |
| 1 | AIC | Wait(1)→Cube NTT mat→Set(3) |
| 1 | AIV | Set(1)；Wait(3)；AIV0 `ComputeNttAndSuDot`；AIV1 仅握手 |
| 2 | AIC | Wait(1)→Cube INTT mat→Set(3) |
| 2 | AIV | Set(1)；Wait(3)；AIV0 `ComputeInttAndExtract`→m；写 MAGIC；AIV1 仅握手 |

## Workspace（`d09::`）

```
OFF_S_HAT → OFF_U → OFF_V → OFF_ZETAS → OFF_GAMMAS →
OFF_U_HAT → OFF_W_HAT → OFF_W →
OFF_MAT_A → OFF_MAT_B → OFF_MAT_C_NTT → OFF_MAT_C_INTT →
OFF_TRACE → OFF_M → OFF_PREP_MARK
```

dk/c **不**镜像进 ws；prep 直读 `dkIn`/`cIn`。

## Flag

- `1` = AIV ready → AIC
- `3` = AIC done → AIV
- 禁 `5`/`7`；禁 SoftSync；禁 Wait 环内 SyncAll
