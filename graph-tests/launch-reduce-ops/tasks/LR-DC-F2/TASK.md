# LR-DC-F2 — Decrypt 2→1（prep 融进 MIX）

## 目标

新建独立用例 `dec_related/RB-D09-decrypt-1launch`（**禁止改 D08/T28/T29 源码**）：

Host：**单次** `ACLRT_LAUNCH(d09_decrypt_fused_custom)` → `m[32]`≡liboqs；NPU×30。

## 方案要点

- AIV0 前缀 `PrepUnpackDecrypt`（直读 H2D；禁 ws SetValue 镜像）
- 全核 `SyncAll` 仅在 CrossCore Wait 环外
- 复用 flag `{1,3}` 两轮：NTT+dot → INTT+extract
- Compress₁：统一整数常数 `C=41285357`
- Magic out：`0x44303931`（`D091`）

## 关键风险

同核串行两段 MIX 的 flag 生命周期；Wait 环内禁 SyncAll；NPU 业务写出禁 `GlobalTensor::SetValue`。

## cannbot

上板前 `sync_audit`；挂则停并写证伪，不硬融。

## runner

`main_npu` only。
