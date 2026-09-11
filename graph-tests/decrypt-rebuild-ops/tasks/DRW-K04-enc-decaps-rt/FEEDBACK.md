```
ID: PASS_CPU
cmd: cd graph-tests/dec_related/RB-D07-enc-decaps-rt && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈39s；kernel wall_sec=9.810 / budget=900）
sync_audit: clean（无红线；SYNC-05×3 薄封装假阳性同前刀；SYNC-09 性能）
notes:
- 新建 `RB-D07-enc-decaps-rt/`；七核 basename 唯一：enc_g/enc_prep/enc_mix/dec_prep/dec_ntt_dot/dec_intt_extract/decaps_fo。
- Host 单 session 多 launch + 每次 mid-sync；禁两段 aclFinalize；BLOCK_DIM=1；flag∈{1,3,4}；X12。
- Encaps：enc_g(m‖h)→(K,r)→enc_prep→enc_mix→c；Decaps：Decrypt 三 launch→G→ReEnc（共用 enc_*）→FO→K_dec。
- 验收：K_dec≡K_enc≡liboqs golden_k max=0；c_enc≡golden_c（诊断）max=0。
- 假绿三问：①golden=liboqs 非设备同源；②cross_backend=liboqs；③多 launch mid-sync，往返 K 整块绿。
- 未跑 NPU/×30/SSH；未改 Decrypt KB/DAG；未抄 T25–T27/alg15|21/examples/frozen/l18。
next_hint: 请主控 `-r npu` ×1 再 ×30（预算 180）关 Q-RT-HANG
```

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：七核往返；`K_dec≡K_enc≡liboqs` max=0。
- **NPU 首轮**：侧树编译因 `AllocSz` 仅 CPU 使用 → `-Werror=unused-function` **失败**；空轮询属主控失误（已停）。
- **修复**：`AllocSz`/`kMinAlloc` 包进 `#ifdef ASCENDC_CPU_DEBUG`；本机 CPU 复验绿。
- **NPU 重跑**（`cannlab-npu` `100.109.67.103`）：×1 + **×30 ok=30 fail=0**；kernel ≈2.2–3.3s / 预算 180。

| 结论 | **关 `Q-RT-HANG` / DG7**；Decrypt/Decaps 战役收口；**已停心跳放机** |

ID: PASS_NPU_X30  
exit: 0  
stress: ok=30 fail=0

## 假绿三问（书面）

1. **golden 是否与实现同源？** — 否。`golden_k`/`golden_c` ← `liboqs_kem_ref` Encaps；设备路径独立；缺库 BLOCKED。
2. **权威交叉是否已跑？** — 是。`cross_backend.txt=liboqs`；`K_enc`/`K_dec` 均对拍 golden_k。
3. **跨核写读是否有 Sync？** — 是。每 launch 后 Host mid-sync；Encrypt/Decrypt MIX 面 flag 1/3+4；往返 K 整块绿（非半边错）。

## Basename 检查单（S0B §6）

- [x] 同 binary 七核 basename 全局唯一（无裸 `prep_custom`）
- [x] Decrypt：`dec_prep` / `dec_ntt_dot` / `dec_intt_extract`
- [x] Encaps/ReEnc 共用：`enc_prep` / `enc_mix`（非 reenc_* 撞名，亦非 T25–T27 文件名）
- [x] G：`enc_g_custom`（Encaps+Decaps 共用）
- [x] FO：`decaps_fo_custom`
- [x] 未从 RB-T25/26/27 复制文件名

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| 用例 STATUS | [`../../../dec_related/RB-D07-enc-decaps-rt/STATUS.md`](../../../dec_related/RB-D07-enc-decaps-rt/STATUS.md) |
