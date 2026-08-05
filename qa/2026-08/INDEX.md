# qa/2026-08 — 月索引

| 日期 | 文件 | 一行摘要 |
|------|------|----------|
| 2026-08-05 | [2026-08-05-l18卡死初步诊断与实机最小实验.md](2026-08-05-l18卡死初步诊断与实机最小实验.md) | encaps/decaps 卡在 `l18_l19` SynchronizeStream：收束为 MIX CrossCore；假设序 H-pollute→GATE→INTT→NTT；≤4 刀实机 + `F203_L18_TRACE` 读法 |
| 2026-08-03 | [2026-08-03-实机device1-l18复跑死锁.md](2026-08-03-实机device1-l18复跑死锁.md) | **订正**：非「1 号卡坏」=同卡脏退污染；`DeviceGuard`+LUT 硬失败+`F203_L18_TRACE` 已推；Cloud 按纪要 §4 实机排 alg19/l18 主因 |
