# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-08（T27 NPU 往返临时绿；下一步 **整段重写 Decrypt/Decaps**）

## ★ 60 秒

1. Encaps/Encrypt：**已双绿 + liboqs 交叉 + ×30**（T22–T24 / Q-ULT）。  
2. 临时树 **T25–T27**：设备 Decrypt/Decaps/往返；**T27 NPU 已绿**（根因：双 `prep_custom.cpp` auto_gen 撞名 → `dec_prep` 未进 `device_aiv.o` → 507000）。  
3. **用户计划**：Decrypt/Decaps **整段从头重写**（现 T25–T27 不当长期实现）。  
4. 战役预算默认 **180s**；NPU 用 `which_npu.sh`（勿写死主机名）。  
5. Git：`chore/thirdparty-add-cannbot-skills`。

## 下一刀

| 序 | 刀 | 说明 |
|----|-----|------|
| 重写 | Decrypt → Decaps → 设备往返 | 禁抄旧/现临时树；防撞名；反卡死+X12+liboqs |
| （可选） | T26 NPU / T27×N 压测 | 仅临时验证；可随重写废弃 |
