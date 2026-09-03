# Decrypt hang — 排查计划（从图谱导出）

> 主控维护。图谱：`docs/rg-kem-decrypt-hang.yaml` · Viewer：`docs/rg-kem-decrypt-hang.html`  
> **不是** Decaps K=131（那是 `DECRYPT_K131_PLAN.md` / `rg-kem-decrypt-k131.yaml`）。  
> **不是** Encaps 粘性（`rg-kem-encrypt-hang.yaml`）。  
> 同时仅一个 subagent；迭代门禁 = **SIM**；**未沉握手机制前不请用户上机**。

---

## 1. 用户要的事

实机 **PKE Decrypt**（`stable-fips203-mlkem-pke-decrypt-k4`）在

```text
prod input = dk_pke + c + lut_* only; fixture under output/_gen_fixture/
```

之后卡死（kernel 不返回），不是「跑完 K≠golden」。

工作法对齐 Encrypt hang：主控画图/设计实验；subagent 在 `ascendc-tests/` 做 toy；失败一等公民刷新图谱。

---

## 2. 已沉淀事实（W0，源码 + 既有 SIM）

| 节点 | 含义 |
|------|------|
| `F-case-is-pke-decrypt-fused` | 单 MIX `f203_decrypt_device_fused` |
| `F-softsync-two-slots` | AIV0 写哨兵、AIV1 自旋；slot0=prep，slot1=su_dot+pad |
| `F-aic-entry-wait4` | AIC **入口** `Wait(4)` 再 `Set(8)` |
| `F-two-gate-two-cube` | 两轮 GATE + NTT Cube + INTT Cube |
| `F-ntt-flags-123-intt-13` | INTT **不用** flag 2 |
| `F-host-zeros-softsync` | Host 必须清零，否则 AIV1 死等 |
| `F-no-device-trace` | 无 Encaps 级 TRACE |
| `F-cloud-sim-fused-pass` | Cloud 全量 SIM 曾绿 → hang 按 NPU-only，toy 用故障注入挂 |
| `F-encrypt-omit-set4-hangs-sim` | Encaps toy 已证：缺 SET(4) ⇒ SIM 124 |
| `F-encrypt-falsified-dual-cube-only` | **禁止**再把「双 Cube」当充分 hang 因请用户上机 |

---

## 3. 生产握手（toy 必须同构的骨架，不要真算法）

```text
AIV0: stub_prep → Arrive(slot0)
AIV1: 自旋等 slot0
双 AIV: SET(4) → WAIT(8) → Clear(slot0)
NTT-like: SET(1) / AIC MMAD / WAIT(3)
AIV0: stub_dot → Arrive(slot1)
双 AIV: SET(4) → WAIT(8) → Clear(slot1)
INTT-like: SET(1) / AIC MMAD / WAIT(3)   # 不要 SET/WAIT(2)
magic → out
AIC: 入口 WAIT(4)→SET(8) → WAIT(1) MMAD SET(3) → WAIT(4)→SET(8) → WAIT(1) MMAD SET(3)
```

---

## 4. 假说阶梯（勿跳；失败也要入库）

| 顺序 | 假说 | 预期 |
|------|------|------|
| **T0** | 合法握手 stub 链 | SIM **绿**、快、magic |
| **T1** | `J-omit-set4-hangs-decrypt` | 合法 SoftSync 后 **省略 SET(4)** → **124** |
| **T2** | `J-omit-slot0-spin-hangs` | AIV0 不 Arrive(slot0) → **124** |
| **T3** | `J-omit-slot1-hangs-after-ntt` | 第一轮 Cube 后缺 slot1/SET(4) → **124** |
| **T4** | `J-dirty-softsync-hang-vs-race` | 预填 1 更像误放行；预填 0 像清零 |
| **T5** | 拉长 stub prep 仍绿 | 支持「忙等≠死锁」 |
| 勿排 | `J-dual-cube-sufficient-hang` | **已 retracted** |

未完成 T0–T2 的 SIM 沉积，**不准**把「请到 NPU 跑全量 Decrypt」写成下一刀。

---

## 5. 实验落点

| 目录 | 用途 |
|------|------|
| `ascendc-tests/fix-decrypt-skel-mix-chain-toy/` | Decrypt fused **握手 toy**（本线主交付） |
| `graph_tests/_inbox` / `_outbox` | TASK / FEEDBACK |
| `examples/stable/.../pke-decrypt-k4` | **非**日常改码场 |

壳可参考活跃 `fix-encrypt-skel-mix-chain-toy` 的 CMake/`run.sh`/轻量 MMAD；**kernel 必须按上表 Decrypt 握手重写**（含 SoftSync，这是本线与 Encrypt toy 的差）。禁止 `frozen/`。

---

## 6. 与另外两张图的边界

| 可借 | 不可混 |
|------|--------|
| Encaps：缺 SET(4)⇒124；禁 SyncAll@Wait；禁滥 launch | 把 Encaps Hostμ / skipNtt 当 Decrypt 拓扑 |
| SoftSyncArrive 生产定式 | 把 K=131 对拍错当 hang |
| FORCE 纪律、粘性残留假说（更后） | 并行 SIM；同 TASK 改三条线 |
