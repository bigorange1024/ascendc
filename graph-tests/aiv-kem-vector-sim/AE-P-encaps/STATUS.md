# AE-P-encaps · STATUS

> 日期：2026-09-12  
> 结论：**AE-P2 PASS（全 AscendC）**— cpu+`SIM_DIRECT=1` sim 下 `c`与`K`≡liboqs max=0  
> 同 launch：**DEVICE_FO + CBD + SampleNTT + ByteDecode + Encrypt**；Host 仅 `ek|m` + 表

## 1. 形态

| 项 | 内容 |
|----|------|
| FO | 设备：`H(ek)=SHA3-256`、`G(m‖H)=SHA3-512` → 写 `K[32]` |
| CBD | 设备：`SHAKE256(coins‖nonce)` + Alg.8 η=2 → y/e₁/e₂ |
| SampleNTT | 设备：SHAKE128(ρ‖j‖p)+Alg.7 ×16 → Â |
| ByteDecode₁₂ | 设备：shared 标量 ×4 → t̂ |
| 变换 | 同 AE-E：NTT/matvec/INTT/compress/pack → `c` |
| 权威 | `liboqs_kem_fixture` Encaps（SEED_D=20260619） |

输入契约：`(ek,m)+表 → (c,K)`；workspace 由设备写。

## 2. 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | c | K | tick |
|------|------|---|---|------|
| CPU | 0 | max=0 | max=0（DEVICE_FO） | wall≈2.6–3.4s |
| SIM | 0 | max=0 | max=0 | **Total tick = 1977873**；wall≈440s |

日志（少 launch · SIM 战役）：`/opt/cursor/artifacts/low-launch-sim/aiv-ae-p-{cpu,sim}.log`  
Host launch 审计：`ACLRT_LAUNCH_KERNEL` ×**1**（`main.cpp`）；反馈见 `graph-tests/encrypt-encaps-low-launch/tasks/A2-aiv-encaps/FEEDBACK.md`

## 3. 已关闭残差

- ~~设备 SampleNTT(Â) / ByteDecode₁₂(t̂)~~ → **已完成**（本 STATUS）
