# STATUS — fix-f203-alg14-encrypt-2launch-k4

**定位**：FIPS 203 Alg.14（ML-KEM-1024 PKE.Encrypt，k=4）设备全链正确性探针，
**按 keygen 蓝本从零重建**（单 ACL session + 少量 MIX launch）。见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)、[`SELF_CONTAINED.md`](SELF_CONTAINED.md)。

**阶段**：**重建启动**（2026-06-30）。骨架 + 蓝图就绪；设备代码逐 Gate 搭建中。

## 重建动因

旧探针 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/)：
CPU 全链 PASS，但 SIM `c.bin` 阻塞（多段 ACL session + NTT 后 launch AIV-only 核 → `507000` + `free(): invalid pointer`）。
根因与解法见 `INTEGRATION_PLAN.md` §0。**MIX 路线未冻结**（已核实，活跃 keygen `mmad_custom` 即 MIX，SIM ✅）。

## Stage Gate 进度

| Gate | 内容 | 设备 | golden | 状态 |
|------|------|------|--------|------|
| 骨架 | 目录 + 蓝图文档 | — | — | ✅ |
| G0 | launch/session 壳（L1 AIV + L2–L5 MIX 壳） | ☐ | ☐ | 待做 |
| G1 | prep：ρ→â、coins→r,e₁,e₂、ek→t̂ | ☐ | ☐ | 待做 |
| G2 | NTT(r)→r̂ | ☐ | ☐ | 待做 |
| G3 | û=Âᵀ·r̂、tr̂=t̂·r̂ | ☐ | ☐ | 待做 |
| G4 | INTT + e₁/e₂ + Decompress₁(m)→u,v | ☐ | ☐ | 待做 |
| G5 | Compress₁₁/₅ + ByteEncode→c | ☐ | ☐ | 待做 |
| 全链 | CPU+SIM c.bin 1568B max=0 | ☐ | ☐ | 待做 |

## 关键铁律（详 INTEGRATION_PLAN §3）

1. 单 ACL session（一次 aclInit/aclFinalize）。
2. L1 prep = `AIV_ONLY` 最先 launch；L2–L5 = `MIX_AIC_1_2`。
3. 每个 MIX 核单文件单 kernel（避 auto_gen 降级）。
4. CPU `#ifdef ASCENDC_CPU_DEBUG` 走 AIV_ONLY；SIM/NPU 走 MIX（Twin Path 共用 golden）。

## 资产

- Python golden（黑盒 oracle）：`scripts/host_golden/f203_ref_common.py`（FIPS 203 NTT/INTT/basemul/Compress/ByteEncode/embed，已验证）+ 待 vendored `gate_g1/g2/g3.py`、`golden_c.py`。
- 计算核：从旧 encrypt 探针（CPU PASS）+ keygen vendored 复制后重组 launch 编排。
