# FROZEN — fix-f203-alg19-kem-keygen-correctness-k4（2026-07-20 关闭）

**判决日期**：2026-07-20  
**原路径**：`ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/`

## 原角色

FIPS 203 **Alg.19 ML-KEM.KeyGen**（k=4）**正确性 / oracle 路标**：vendor 拼装 PKE KeyGen + KeyGen_internal 尾；早期 correctness 基准（CPU+SIM+liboqs PASS；SIM 曾记 **~742k**）。

## 关闭原因

| # | 原因 |
|---|------|
| 1 | **作为正确性验证测试基准的任务已完成** |
| 2 | **正式交付已齐**：[`stable-fips203-mlkem-kem-keygen-k4`](../../../examples/stable/stable-fips203-mlkem-kem-keygen-k4/)；行为基线 [`pass-fix-f203-alg19-kem-keygen-device-k4`](../../pass-fix-f203-alg19-kem-keygen-device-k4/) |
| 3 | 继续保留活跃 `fix-*-correctness` 易被 Agent 当实现参考 / 跑 CI，与「只认 stable + pass-fix」冲突 |

## 合法继任（禁止从本目录抄码）

| 能力 | 路径 |
|------|------|
| **定型交付 KEM KeyGen** | [`examples/stable/stable-fips203-mlkem-kem-keygen-k4/`](../../../examples/stable/stable-fips203-mlkem-kem-keygen-k4/) |
| 设备行为基线（只读对照） | [`pass-fix-f203-alg19-kem-keygen-device-k4`](../../pass-fix-f203-alg19-kem-keygen-device-k4/) |

## Agent 规则

- **可进入**只读本 `FROZEN.md` / `STATUS.md` 了解关闭原因与继任
- **禁止**打开本树其它源码作实现模板；**禁止** `vendor_sync` / `run.sh` 验收；**禁止**把本目录写进新脚本默认路径或 INDEX 活跃表
- 仓库 `scripts/` KeyGen 默认已指 **pass-fix device**（或 stable），与本目录无关
