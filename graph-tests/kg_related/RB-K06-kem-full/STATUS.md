# STATUS — RB-K06-kem-full

| 字段 | 值 |
|------|-----|
| 刀 | KGR-K02 · KEM KeyGen 四 launch 全链 + liboqs |
| 状态 | **PASS_CPU**（npu: **wait_npu**） |
| 日期 | 2026-09-09 |
| 墙钟 | ~24s（编译+CPU；kernel≈5.4s） |

## 目标达成

1. Host 四 launch：`kg_prep_custom` → mid-sync → `kg_ntt_custom` → sync → `kg_dot_encode_custom` → sync → `kg_kem_tail_custom`
2. basename 唯一；L1 **链接** `RB-K01/kg_prep_custom.cpp`；L2a/L2b/L3 本目录拼装壳 + `k02_inc`/`k03_inc`
3. I/O：`seed_d` → `ek[1568]` / `dk_kem[3168]`（`dk_kem=dk_pke‖ek‖H(ek)‖z`）
4. 权威对拍：**liboqs_kem_ref** KeyGen（`SEED_D=20260619`；z 域分离对齐 `liboqs_kem_fixture`）；缺库则 BLOCKED（本机有库）
5. L3 AIV-only、`BLOCK_DIM=1`；禁与 L2b 深 CrossCore 融合；禁抄 KeyGen/alg19/frozen

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；ek/dk_kem **max=0**；PASS_SYNC+PASS_IO |
| Oracle | `/home/yuanye/ascendc/scripts/liboqs_kem_ref` |
| Seed | `SEED_D=20260619`；`kem_seed=d‖z` |
| sync_audit | clean（无红线；SYNC-05×2 薄封装假阳性 + SYNC-09 性能） |
| SIM / NPU | 本刀未跑；**npu: wait_npu**（云机关机） |

运营回报：[`../../keygen-rebuild-ops/tasks/KGR-K02-kem-full/FEEDBACK.md`](../../keygen-rebuild-ops/tasks/KGR-K02-kem-full/FEEDBACK.md)

## 布局速记

- L1→L2a：Host 拷 ŝ/ê；L2a→L2b：Host 拷 Â+ŝ̂/ê̂；ρ/ζ/γ/mat 预喂
- L2b→L3：Host mid-sync 后喂 ek/dk_pke；L3 设备算 H(ek)+z 并拼 dk_kem
- `ek = BE₁₂(t̂)[1536]‖ρ[32]`；`dk_kem = dk_pke‖ek‖H(ek)‖z`
