# LAYOUT — RB-T30-decaps-2launch

## Host（2 launch）

```
Load dk_kem[3168], c[1568], zetas, gammas, mat_a/b
    │
    ▼
L1 ACLRT_LAUNCH(t30_dec_fused_custom)   ← Decrypt 全链 MIX
    │  Sync → D2H m'[32]
    ▼
L2 ACLRT_LAUNCH(t30_enc_fused_custom)   ← Re-Encrypt/Encaps 全链 MIX
    │  Sync → D2H c'[1568], K'[32]
    ▼
Host FO：c'≡c ? K' : J(z‖c) → K[32]
    │
    ▼
落盘 out magic 0x54333032 · launch_count=2 · TRACE
```

## L1 Device（Decrypt）

| 段 | 角色 | 动作 |
|----|------|------|
| 0 | AIV0 | Unpack：dk/c→ŝ/u/v（直读 H2D；UB+DataCopy） |
| 0b | ALL | `SyncAll()`（Wait 环外） |
| 1 | AIC/AIV | flag 1/3：Cube NTT；AIV0 NTT(u)+⟨ŝ,û⟩ |
| 2 | AIC/AIV | flag 1/3：Cube INTT；AIV0 INTT+Compress₁→m' |

## L2 Device（Encaps/Reenc）

| 段 | 角色 | 动作 |
|----|------|------|
| 0 | AIV0 | prep：ek→ws + G + μ + Decode₁₂ + CBD + SampleNTT(Â) |
| 0b | ALL | `SyncAll()`（Wait 环外） |
| 1 | AIC/AIV | flag 1/3：Cube NTT；AIV0 NTT(y)+Mul |
| 1b | AIV→AIC | flag 4=GATE |
| 2 | AIC/AIV | flag 1/3：Cube INTT；AIV0 INTT+噪+pack→c' |

## Flag

- `1` = AIV ready → AIC；`3` = AIC done → AIV；`4` = GATE（仅 L2）
- 禁 `5`/`7`；禁 SoftSync；禁 Wait 环内 SyncAll
