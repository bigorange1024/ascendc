# FROZEN — fix-f203-alg15-pke-decrypt-correctness-k4（2026-07-10 关闭）

**判决日期**：2026-07-10  
**原路径**：`ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/`

## 原角色

FIPS 203 **Alg.15 K-PKE.Decrypt**（ml_kem_1024 / k=4）**正确性探针**（G4）：**2 launch**（prep ‖ ntt+intt），`m.bin` CPU+SIM max=0；曾作 round-trip / liboqs Decrypt 回退路径。

## 关闭原因

| # | 原因 |
|---|------|
| 1 | **正确性验证任务已完成**（G4 双模式 PASS；SIM **~427k**） |
| 2 | **交付算子已晋级** [`stable-fips203-mlkem-pke-decrypt-k4`](../../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/)（1-kernel）；仓库脚本默认 Decrypt 已指向 stable |
| 3 | 生产优化路径另有 [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../pass-fix-f203-alg15-pke-decrypt-device-k4/)；本目录仅作历史 2-launch 正确性归档 |

## 合法继任（禁止从本目录抄码）

| 能力 | 路径 |
|------|------|
| **定型交付 Decrypt** | [`examples/stable/stable-fips203-mlkem-pke-decrypt-k4/`](../../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) |
| 优化全链探针（对照） | [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../pass-fix-f203-alg15-pke-decrypt-device-k4/) |

## Agent 规则

- **可进入**读本 `FROZEN.md`、`STATUS.md` 了解 2-launch 正确性切分历史
- **禁止**作 `DECRYPT_DIR` 默认或回退推荐；**禁止**跑本目录 CI 验收；**禁止**把本树当新实现模板改写后「复活」
- **例外（历史）**：曾允许已冻结的 [`frozen-fix-…-alg21-kem-decaps-correctness-k4`](../frozen-fix-f203-alg21-kem-decaps-correctness-k4/) `vendor_sync` rsync 本树（2-launch G4）。**2026-07-20 起该 correctness 已冻结**；**禁止**为新工作复活 sync / 抄本树。
