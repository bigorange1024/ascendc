# FROZEN — fix-f203-alg20-kem-encaps-correctness-k4（2026-07-20 关闭）

**判决日期**：2026-07-20  
**原路径**：`ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4/`

## 原角色

FIPS 203 **Alg.20 ML-KEM.Encaps**（k=4）**正确性 / oracle 路标**：vendor Encrypt（G5←frozen alg14）；早期 correctness 基准（CPU+SIM PASS）。

## 关闭原因

| # | 原因 |
|---|------|
| 1 | **作为正确性验证测试基准的任务已完成** |
| 2 | **正式交付已齐**：[`stable-fips203-mlkem-kem-encaps-k4`](../../../examples/stable/stable-fips203-mlkem-kem-encaps-k4/)；行为基线 [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../pass-fix-f203-alg20-kem-encaps-device-k4/) |
| 3 | 活跃 `fix-*-correctness` 易诱使 Agent 翻 vendor 拼装码；生产路径已是 stable / pass-fix |

## 合法继任（禁止从本目录抄码）

| 能力 | 路径 |
|------|------|
| **定型交付 KEM Encaps** | [`examples/stable/stable-fips203-mlkem-kem-encaps-k4/`](../../../examples/stable/stable-fips203-mlkem-kem-encaps-k4/) |
| 设备行为基线（只读对照） | [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../pass-fix-f203-alg20-kem-encaps-device-k4/) |

## Agent 规则

- **可进入**只读本 `FROZEN.md` / `STATUS.md`
- **禁止**抄 `vendor/` / kernel；**禁止**作 `ENCAPS_DIR` 默认
- **2026-08-19 用户授权例外**：为借入实机测教材 A 臂，**原地**改 `run.sh` / host 计时壳（对齐 stable npu + `MSPROF_MODE=app` + `[npu_launch]`）。仍**禁止**把 kernel/vendor 抄出活跃树。
- 历史 `vendor_sync_from_alg14_encrypt.sh` 仅归档自用；**禁止**为新工作复活 sync
