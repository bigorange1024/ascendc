# 2026-09-12 · 单 AIV 全向量 Encrypt/Encaps（SIM）

## 结论（主控独立复验）

- **Encrypt（AE-E2）PASS**：单 AIV、单 launch、`c`≡liboqs max=0；SIM tick≈1200108。
- **Encaps（AE-P2）强 PASS**：同 launch 内 DEVICE_FO（SHA3 H/G）+ DEVICE_CBD + Encrypt；`c/K`≡liboqs；SIM tick≈1384945。
- 积木：AV01 系向量 NTT；新建向量 INTT；向量 matvec/dot；pack；设备 Keccak FO。
- 残差：Â 的 SampleNTT×16、t̂ ByteDecode₁₂ 仍可 Host；未上 NPU。
- 图谱/KB 已回写；**未** git 分支/commit/push。

## 本轮再验（会话续跑，无代码改动）

| 用例 | CPU | SIM | tick |
|------|-----|-----|------|
| AE-E | PASS c max=0 | PASS | ≈1200017 |
| AE-P | PASS c/K max=0 | PASS | ≈1384867 |

日志：`/opt/cursor/artifacts/aiv-kem-ae-{e,p}-{cpu,sim}.log`；用例根无 stray dump。  
用户指令：继续到 Encrypt/Encaps 正确为止；**禁擅自分支/推送**——遵守，未 commit。

## 全 AscendC（L8 加锁后）

Host 不再预喂 Â/t̂/y/e。主控复验：

| 用例 | 输入 | CPU | SIM tick |
|------|------|-----|----------|
| AE-E | ek\|m\|coins | c max=0 | ≈1885458 |
| AE-P | ek\|m | c/K max=0 | ≈1977711 |

资产清单：`graph-tests/aiv-kem-vector-sim/ASSETS.md`。  
架构收益（用户确认）：单 AIV 可串多例、无组件同步；多 AIV = 多任务并行。

## 假绿三问

1. golden 同源？否——权威 liboqs fixture。  
2. 权威交叉？是——cpu+SIM 均 max=0。  
3. 跨核 Sync？AIV-only，无 CrossCore；sync_audit 红线 0。
