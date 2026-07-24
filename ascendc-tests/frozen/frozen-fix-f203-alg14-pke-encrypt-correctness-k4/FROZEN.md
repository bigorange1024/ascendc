# FROZEN — fix-f203-alg14-pke-encrypt-correctness-k4（2026-07-10 关闭）

**判决日期**：2026-07-10  
**原路径**：`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`

## 原角色

FIPS 203 **Alg.14 K-PKE.Encrypt**（ml_kem_1024 / k=4）**正确性拼装探针**（G5）：多 launch 设备全链，`c.bin` CPU+SIM max=0；曾作 liboqs / round-trip Encrypt 段默认。

## 关闭原因

| # | 原因 |
|---|------|
| 1 | **正确性验证任务已完成**（G5 双模式 PASS；SIM tick 曾记 **922441**） |
| 2 | **交付算子已晋级** [`stable-fips203-mlkem-pke-encrypt-k4`](../../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)；仓库脚本默认 Encrypt 已指向 stable |
| 3 | 继续保留活跃 `fix-*` 易与 stable / `pass-fix-…-encrypt-device-k4` 混淆，增加幽灵引用 |

## 合法继任（禁止从本目录抄码）

| 能力 | 路径 |
|------|------|
| **定型交付 Encrypt** | [`examples/stable/stable-fips203-mlkem-pke-encrypt-k4/`](../../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) |
| 优化全链探针（对照） | [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../../pass-fix-f203-alg14-pke-encrypt-device-k4/) |

## Agent 规则

- **可进入**读本 `FROZEN.md`、`STATUS.md`、历史 `G3_SIM_AUDIT.md` 等关闭说明
- **禁止**作 `ENCRYPT_DIR` / KAT / roundtrip 默认；**禁止**跑本目录 CI 验收；**禁止**把本树当新实现模板改写后「复活」
- **例外（历史）**：曾允许已冻结的 KEM correctness（现 [`frozen-fix-…-alg20/21-*-correctness-k4`](../frozen-fix-f203-alg20-kem-encaps-correctness-k4/)）`vendor_sync` rsync 本树。**2026-07-20 起 KEM correctness 亦已冻结**；**禁止**为新工作复活 sync / 抄本树。
