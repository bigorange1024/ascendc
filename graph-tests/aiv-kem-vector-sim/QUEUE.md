# QUEUE · aiv-kem-vector-sim

| # | ID | 状态 | 目录/动作 | 门禁 |
|---|-----|------|-----------|------|
| 0 | W0 | **PASS** | KB+图谱+本 QUEUE | rg_validate |
| 1 | AV02 | MERGED | 并入 AE-E（polyvec NTT×4） | — |
| 2 | AV03 | MERGED | 并入 AE-E（向量 INTT 新建） | — |
| 3 | AH01 | **PASS** | 全设备：SampleNTT(Â)+ByteDecode(t̂)+Encrypt CBD | TASK-AE-FULL-ASCENDC |
| 4 | AM01 | MERGED | 并入 AE-E matvec/dot | — |
| 5 | AP01 | MERGED | 并入 AE-E compress/pack | — |
| 6 | AE-E1 | **PASS** | `AE-E-encrypt/` 单 launch | 不挂 |
| 7 | AE-E2 | **PASS** | 纯 `ek\|m\|coins`；DEVICE_FULL；c≡liboqs | cpu+SIM max=0；tick=1885454 |
| 8 | AE-P1 | **PASS** | 设备 FO + Encrypt | DEVICE_FO |
| 9 | AE-P2 | **PASS** | 设备 Â/t̂+FO+CBD；c/K≡liboqs | cpu+SIM max=0；tick=1977777 |
| 10 | AE-FULL | **PASS** | `tasks/TASK-AE-FULL-ASCENDC.md` 已兑现 | 见 AE-E/AE-P STATUS |

**2026-09-12 收口**：密码学步骤全部 AscendC（Host 仅 ek/m/coins + 常量表 + workspace GM）。日志：`/opt/cursor/artifacts/aiv-kem-vector-sim/AE-*-fullascendc-*.log`。
