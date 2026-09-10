# 2026-09-10 · Launch 压缩 · Decrypt→1（RB-D09）· Decaps→2（RB-T30）

## 决策

- 用户要求：**重写实验代码，勿改已有树** → 新建 `RB-D09-decrypt-1launch`，不动 D08/T28/T29。
- Compress₁ 对齐 liboqs 常数路径（非学校式 (2x+q/2)/q）。
- SyncAll 仅在 CrossCore Wait 环外；flag∈{1,3}。

## 结果

- sync_audit：无红线（SYNC-05 遗留同族）。
- NPU 冒烟 PASS；NPU×30 **ok=30 fail=0**（每轮换 SEED_D）。
- QUEUE 1–7 全绿；Decrypt launch 数对齐 stable=1。

## 遗留

- Wave4 经验入库（KB 图改须用户授权）。

---

## T30 · Decaps → 2 Host launch（独立新树）

### 决策

- 新建 `graph-tests/enc_related/RB-T30-decaps-2launch/`；**禁止**改 T28/T29/D08/D09；**禁止**抄核源码。
- Host：**L1** `t30_dec_fused_custom`（Decrypt 全链）+ **L2** `t30_enc_fused_custom`（prep G/CBD/Â…→SyncAll→NTT 路径）+ Host FO。
- magic `0x54333032`；namespace `t30_dec`/`t30_enc`/`rb_t30`；Compress₁ C=41285357。

### 结果（Cloud）

- CPU **PASS**；SIM_DIRECT **PASS**（K≡liboqs、c'≡c、launches=2、TRACE 齐）。
- NPU×30 **未跑**（本机无卡）。

### 风险

- L2 prep 前缀含 SampleNTT/CBD，SyncAll 后 AIV0 工作量仍大（SIM ~400s）；NPU 超时/半写需上板验证。
- Encaps 融合核内 flag 1/3 复用 + GATE4：与 T29 compute 同构握手，但前缀重排后首段 Wait 前无「空握手」，需 NPU 确认。

## NPU 关闸补记

- T30 NPU 冒烟 PASS；×30 ok=30 fail=0。
- QUEUE 1–8 全绿。
