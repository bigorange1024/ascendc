# LAYOUT — RB-K06-kem-full（KEM KeyGen 四 launch）

## Host 编排

```text
L1 kg_prep_custom (AIV)
  → mid-sync
L2a kg_ntt_custom (MIX)
  → sync
L2b kg_dot_encode_custom (MIX)
  → sync
L3 kg_kem_tail_custom (AIV-only, BLOCK_DIM=1)
  → sync → ek[1568] + dk_kem[3168]
```

## I/O

| 阶段 | 输入 | 输出 |
|------|------|------|
| L1 | `seed_d`（32B 垫；前 4B=SEED_D） | Â / ŝ / ê |
| L2a | ŝ/ê + ζ/mat | ŝ̂ / ê̂ |
| L2b | Â + ŝ̂/ê̂ + γ/ρ/mat | ek[1568] / dk_pke[1536] |
| L3 | ek + dk_pke + seed_d | h[32] / z[32] / dk_kem[3168] |

## dk_kem 布局

`dk_kem = dk_pke(1536) ‖ ek(1568) ‖ H(ek)(32) ‖ z(32)`

`z = SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z=<SEED_D>")`（与 `liboqs_kem_fixture` 一致）

## 权威

`scripts/liboqs_kem_ref keygen`；`kem_seed=d‖z`；`SEED_D=20260619`
