# AE-E-encrypt · STATUS

> 日期：2026-09-12  
> 结论：**AE-E2 PASS（全 AscendC）**— cpu + `SIM_DIRECT=1` sim；`c`≡liboqs max=0  
> 输入契约：Host 仅 `ek|m|coins` + NTT 表；Â/t̂/y/e/ŷ 为设备写 workspace

## 1. 目标 / 边界

| 项 | 内容 |
|----|------|
| 参数 | ML-KEM-1024（k=4,du=11,dv=5,η=2） |
| 形态 | `KERNEL_TYPE_AIV_ONLY`，blockDim=1，**单 launch** |
| 禁令 | 无 Cube / CrossCore；未抄 frozen |
| **DEVICE_FULL** | CBD + SampleNTT×16 + ByteDecode₁₂ + NTT/matvec/INTT/pack 均在设备 |
| 权威 | `liboqs_pke_ref`；接线前 host `golden_encrypt`≡liboqs（不喂核） |

## 2. 积木

| 能力 | 实现 |
|------|------|
| CBD | SHAKE256(coins‖nonce)+Alg.8 η=2 → y/e₁/e₂ |
| SampleNTT | SHAKE128(ρ‖j‖p)+Alg.7 rej ×16 → Â |
| ByteDecode₁₂ | shared `poly_byte_decode12_scalar_gm` ×4 → t̂ |
| 正向 NTT | AV01 Gather 蝶形 ×4 poly 同 launch 串行 |
| INTT | 逆根 + 逆置换 + CT 逆蝶形（×2^{-1}/层） |
| matvec/dot | Alg.11 偶奇 Gather + Vec Mul/Add |
| pack | Compress₁₁/₅ + ByteEncode |

## 3. 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | max diff | tick / wall |
|------|------|----------|-------------|
| CPU | 0 | **c max=0** | wall≈2.6s |
| SIM | 0 | **c max=0** | **Total tick = 1885454**；wall≈386s |

日志：`/opt/cursor/artifacts/aiv-kem-vector-sim/AE-E-fullascendc-{cpu,sim}.log`

## 4. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py aiv_encrypt.cpp
```

- 历史红线 0；本刀未改 NTT/Sync 骨架（仅前置采样）

## 5. 已关闭残差

- ~~设备 SampleNTT / ByteDecode / CBD~~ → **已完成**（本 STATUS）
