# DRW-S0B · Decaps 刀序草图（可选）

```text
[Q-DEC-CORRECT] ──► K01 RB-D04 G(m'‖h)
                      │
                      ▼
                   K02 RB-D05 Re-Encrypt (双 launch / flag 1,3,4)
                      │
                      ▼
                   K03 RB-D06 FO（合法 + 篡改 c）
                      │
                      ▼
                   K04 RB-D07 设备 Encaps↔Decaps + ×30 @180s
                      │
                      ▼
                 [Q-DECAPS-CORRECT] [Q-RT-HANG]
```

主控近端：S0 双回收 → **DRW-D01 Decrypt prep**（非 K01）。
