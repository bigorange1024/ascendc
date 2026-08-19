# FROZEN — fix-f203-alg21-kem-decaps-correctness-k4（2026-07-20 关闭）

**判决日期**：2026-07-20  
**原路径**：`ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4/`

## 原角色

FIPS 203 **Alg.21 ML-KEM.Decaps**（k=4）**正确性 / oracle 路标**：vendor Decrypt G4 + Encrypt G5 + 设备 FO；早期 correctness 基准（CPU+SIM PASS）。

## 关闭原因

| # | 原因 |
|---|------|
| 1 | **作为正确性验证测试基准的任务已完成** |
| 2 | **正式交付已齐**：[`stable-fips203-mlkem-kem-decaps-k4`](../../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/)；行为基线 [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../pass-fix-f203-alg21-kem-decaps-device-k4/) |
| 3 | 与 device/stable 并存易造成幽灵引用与误跑 vendor 拼装路径 |

## 合法继任（禁止从本目录抄码）

| 能力 | 路径 |
|------|------|
| **定型交付 KEM Decaps** | [`examples/stable/stable-fips203-mlkem-kem-decaps-k4/`](../../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/) |
| 设备行为基线（只读对照） | [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../pass-fix-f203-alg21-kem-decaps-device-k4/) |

## Agent 规则

- **可进入**只读本 `FROZEN.md` / `STATUS.md`
- **禁止**抄码进 device/stable；**禁止**作 `DECAPS_DIR` 默认
- **2026-08-19 用户授权例外**：为借入实机测教材 A 臂，**原地**改 `run.sh` / host 计时壳（对齐 stable npu + `MSPROF_MODE=app` + `[npu_launch]`）。仍**禁止**把 kernel/vendor 抄出活跃树。
- 历史 `vendor_sync_from_alg14/15_*.sh` 仅归档自用；**禁止**为新工作复活 sync
