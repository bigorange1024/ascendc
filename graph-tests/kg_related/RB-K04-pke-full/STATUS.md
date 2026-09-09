# STATUS — RB-K04-pke-full

| 字段 | 值 |
|------|-----|
| 刀 | KGR-P04 · PKE KeyGen 三 launch 全链 |
| 状态 | **PASS_CPU**（npu: **wait_npu**） |
| 日期 | 2026-09-09 |
| 墙钟 | ~23s（编译+CPU；kernel≈5.2s） |

## 目标达成

1. Host 三 launch：`kg_prep_custom` → mid-sync → `kg_ntt_custom` → sync → `kg_dot_encode_custom`
2. basename 唯一；L1 **链接** `RB-K01/kg_prep_custom.cpp`；L2a/L2b 本目录拼装壳 + `k02_inc`/`k03_inc`
3. I/O：`seed_d` → `ek_pke[1568]` / `dk_pke[1536]`
4. 权威对拍：**liboqs_pke_ref** KeyGen（`SEED_D=20260619` Derand）；缺库则 BLOCKED（本机有库）
5. 禁抄 examples/pass-fix/frozen KeyGen；禁融单核

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；ek/dk **max=0**；PASS_SYNC+PASS_IO |
| Oracle | `/home/yuanye/ascendc/scripts/liboqs_pke_ref` |
| Seed | `SEED_D=20260619` |
| sync_audit | clean（无红线；SYNC-05 薄封装假阳性×2 + SYNC-09 性能） |
| SIM / NPU | 本刀未跑；**npu: wait_npu**（云机关机） |

运营回报：[`../../keygen-rebuild-ops/tasks/KGR-P04-pke-full/FEEDBACK.md`](../../keygen-rebuild-ops/tasks/KGR-P04-pke-full/FEEDBACK.md)

## 布局速记

- L1→L2a：Host 拷 ŝ/ê；L2a→L2b：Host 拷 Â+ŝ̂/ê̂；ρ/ζ/γ/mat 预喂
- `ek = BE₁₂(t̂)[1536]‖ρ[32]`；`dk = BE₁₂(ŝ̂)[1536]`
