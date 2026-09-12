# EN15-encrypt-2launch · STATUS

> 日期：2026-09-12  
> 结论：**PASS**（CPU + `SIM_DIRECT=1` sim；`c` vs liboqs **max=0**；Host `ACLRT_LAUNCH_KERNEL` **= 2**）

---

## 1. 目标

把 EN13 的 **8 launch** 积木接线版，新建为 **prep + compute** 两 launch 外形；否决继续以 8 launch 当交付形态。

## 2. 外形

| Launch | 核 | 内容 |
|--------|-----|------|
| L1 | `enc_prep_l1` | SampleNTT(ρ)→Â + CBD(coins)→y |
| Host mid | — | Â→Âᵀ；上传 t̂/γ/e1/e2/μ/M4_ntt/M4_intt（**无** launch） |
| L2 | `enc_compute_l2` | NTT→matvec→dot→INTT×2→加噪→pack→c（阶段间 `SyncAll`；CrossCore **仅 1/2/3**） |

## 3. 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | wall | tick | c max | host_launch |
|------|------|------|------|-------|-------------|
| CPU | 0 | **≈2.2s** | — | **0** | **2** |
| SIM | 0 | **≈153.7s** | **898767** | **0** | **2** |

日志：`/opt/cursor/artifacts/low-launch-sim/en15-cpu.log`、`en15-sim.log`。  
用例根无 stray `core*.dump`；SIM stray 收拢至 `sim_log/`。

## 4. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；K=4；η=2；du=11 dv=5；`blockDim=1`；禁 `-r npu`；禁 GATE 4/8。

## 5. 一条教训

胖 MIX 可在 **Wait 环外 SyncAll** 串级 AIV 积木 + cann-ntt 1/2/3，而不必回到 EN13 的 8×Host launch；Âᵀ/噪声种子仍可 Host mid-sync，只要 launch 计数锁死为 2。
