# FEEDBACK — DRW-S0B-decaps-queue-design

> Subagent 设计回填；只设计不写码；未改 KB/DAG；未碰 SSH/NPU。

```
ID: DESIGN_OK
cmd: review DRW-S0B
exit: 0
wall_min: 18
sync_audit: n/a
notes:
- Decaps 总序：Decrypt 收口(Q-DEC)→K01 G→K02 ReEnc→K03 FO→K04 设备往返+×30。
- T26 判决：五 launch 曾 CPU 绿；禁作源码模板。T27 判决：X12 SetValue→K 错非挂；FO/写出须 DataCopy。
- 禁子进程 liboqs 当生产 Decaps；禁两段 aclFinalize 当基线（X16）。
- basename：dec_* / reenc_* / fo_* 全局唯一；合并 binary 防 507000。
next_hint: S0A+S0B 双绿后主控先派 DRW-D01（Decrypt prep / RB-D01）；Decaps K01 等 Q-DEC-CORRECT。
```

---

## 1. 刀序表（DG4→DG7）

前置（本表外，由 S0A / D-NEAR-FIRST-CODE）：`DRW-D01…D0x` 收口 **Decrypt** → 关 `Q-DEC-CORRECT`。**Decaps 编码刀一律 `deps: Q-DEC-CORRECT`**。

| 建议 ID | 实现目录 | DAG | 依赖 | 形态（设计意图） | 验收命令草案 | liboqs 交叉点 |
|---------|----------|-----|------|------------------|--------------|---------------|
| **DRW-K01** | `graph-tests/dec_related/RB-D04-decaps-G` | `G-DG4-G` | `Q-DEC-CORRECT` | **1× AIV-only** launch：`G(m'‖h)→(K'‖r')`；Host mid-sync 前后清晰；无 CrossCore | Subagent：`bash run.sh -r cpu -v Ascend910B4`；主控：`-r npu`（预算 180s） | Host/`liboqs` 同 `(m',h)` 的 `G`：`K'[32]`、`r'/coins[32]` 逐字节；`h` 须自 `dk_kem` 切片（禁默认可重算 H(ek) 冒充） |
| **DRW-K02** | `…/RB-D05-decaps-reenc` | `G-DG5-REENC` | K01 绿 + `C-ANTI-HANG` | **接 Encaps 拓扑契约** 重拼 Re-Encrypt：建议 **双 launch**（prep/AIV 头 + MIX Encrypt）；flag∈{1,3,4}；`BLOCK_DIM=1`；输入 `ek,m',coins=r'` → `c'[1568]` | 同上 cpu→主控 npu；`sync_audit` 必跑 | `c'` ≡ Encrypt oracle（同 `ek,m',r'`）；可与 host/liboqs PKE Encrypt 对拍；**禁**把临时 T26 树当模板 |
| **DRW-K03** | `…/RB-D06-decaps-fo` | `G-DG6-FO` | K02 绿 | 设备 **FO**：`c≟c'`；选 `K'` 或 `J(z‖c)`；写出 `K[32]`（UB+`DataCopy`，X12） | cpu + 主控 npu；**两路径都跑** | 合法：`K≡liboqs KEM Decaps(dk,c)`；拒绝：篡改 `c` 后 `K≡liboqs Decaps(dk,c_bad)`（即 `J` 路径） |
| **DRW-K04** | `…/RB-D07-enc-decaps-rt` | `G-DG7-RT` | K03 绿 | **设备 Encaps**（只读契约接 T22–T24）↔ **设备 Decaps**（本战役链）同路径；关 `Q-RT-HANG` | cpu 自洽；主控 npu + **×N 压测** | Encaps 出 `(c,K)` ≡ liboqs Encaps；Decaps `(dk,c)→K'` ≡ `K` ≡ liboqs Decaps；往返闭合 |

**Launch 数说明（判决 only，非抄码）**：临时 T26≈Decrypt 三 launch + Reenc 两 launch；T27 再叠 Encaps。本战役 **Decrypt 多 launch 以 S0A 为准**，Decaps 侧按上表分刀，**禁止**把 T26/T27 源码目录 fork 进 `dec_related`。

**Runner 分流**：编码/CPU/SIM→subagent；**全部 `-r npu` / ×N→主控**（COMMON）。

---

## 2. FO 两路径（合法 / 拒绝）与假绿防护

### 2.1 合法路径

| 步骤 | 做法 |
|------|------|
| 密钥 | `liboqs` KeyGen → `dk_kem[3168]` / `ek`；同对固定进 input |
| 密文 | 设备或权威 Encaps（同 `m`）→ `c[1568]`；或 host/liboqs Encaps 喂入（K03 可先用权威 `c`） |
| 期望 | `K_dev ≡ liboqs_kem_decaps(dk,c) ≡ Encaps 的 K`（若同会话有 Encaps） |
| 内部不变量（诊断） | 合法时必有 `c'==c`，故选 `K'`；`c'≠c` 则 FO 实现或 Re-Encrypt 错 |

### 2.2 拒绝路径

| 步骤 | 做法 |
|------|------|
| 造向量 | 合法 `c` 上 **确定性篡改**（如翻转 `c` 末字节 / `c1` 内一比特）；`dk`/`z` 不变 |
| 期望 | `K_dev ≡ liboqs_kem_decaps(dk, c_tampered)`（应为 `J(z‖c_tampered)`，**≠** 合法 `K'`） |
| 对照 | 另算 host `J(z‖c_bad)` 诊断；**权威仍以 liboqs Decaps 为准** |
| 通过条件 | 两路径均绿；拒绝路径 `K` 与合法路径 `K` **必须不同**（防「恒输出 K'」假绿） |

### 2.3 假绿三问（K03/K04 强制书面答）

1. **golden 是否与实现同源？** — 禁止 python Decaps / 自写 `J` 冒充权威；golden 仅作诊断。  
2. **权威交叉是否已跑？** — 缺 aarch64 liboqs → **BLOCKED**（X11），不得报 PASS。  
3. **跨核写读是否有 Sync？** — 仅半边/`c'` 错时优先查 mid-sync / CrossCore（deadlock-triage / X1）；`K` 整块错且未挂 → 优先 X12 写出。

**另禁**：子进程调 liboqs 当「生产 Decaps 实现」；同 binary 自洽对拍无 liboqs；拒绝路径未断言 `K≠K_legit`。

---

## 3. 与 Decrypt 接口（GM 契约）

`dk_kem[3168] = dk_pke[1536] ‖ ek[1568] ‖ h[32] ‖ z[32]`（与 liboqs / KB §A2）。

| 符号 | 尺寸 | 来源 / 生命周期 | 消费方 | 写出约束 |
|------|------|-----------------|--------|----------|
| `dk_pke` | 1536 | `dk_kem[0:1536)`；Decrypt 输入 | Decrypt prep | H2D 直读；禁 SetValue 镜像业务 GM（X12） |
| `c` | 1568 | Encaps / 测试向量；**FO 比对只读原件** | Decrypt unpack + FO `c≟c'` + `J(z‖c)` | 篡改向量仅拒绝路径替换 |
| `m'` | 32 | Decrypt INTT/extract **终产物** | K01 `G(m'‖h)`；K02 Encrypt μ 源 | **UB+DataCopy→GM**；Host mid-sync 后对下一 launch 可见 |
| `ek` | 1568 | `dk_kem[1536:3104)` | K02 Re-Encrypt | 切片或独立 GM；生命周期贯穿 Reenc |
| `h` | 32 | `dk_kem[3104:3136)` | K01 `G`；**不强制**设备重算 H(ek) | 只读切片 |
| `z` | 32 | `dk_kem[3136:3168)` | K03 `J(z‖c)` | 只读；拒绝路径必用 |
| `K'` | 32 | K01：`G` 输出高/前 32B（与 Encaps 约定对齐即可，TASK 锁死一种） | FO 合法选路；诊断 | DataCopy 写出 |
| `coins` / `r'` | 32 | K01：`G` 输出另 32B | K02 CBD/PRF；**Host 禁预喂**（对齐 T22 思路） | DataCopy；Reenc 只读 |
| `c'` | 1568 | K02 pack 终产物 | FO 比对 | DataCopy；与 `c` 同布局 |
| `K` | 32 | K03 FO 选路写出 | Host / K04 往返 | DataCopy；权威交叉对象 |

**会话**：Decrypt 链结束 → Host barrier → 再启 G / Reenc（多 launch + mid-sync）；**禁止**「同超长 session 无屏障 Decrypt→Encrypt」当基线（X16）；**禁止**两段 `aclFinalize` 当生产解法。

---

## 4. Re-Encrypt：接 Encaps 拓扑契约（不抄 T22 源码）

可复用 **契约条目**（来自 Encrypt KB §A2/§B2、T22–T24 FEEDBACK 思路）：

| # | 契约 | Decaps Re-Encrypt 用法 |
|---|------|------------------------|
| 1 | **多 launch**：AIV-only 头与 MIX Encrypt 拆开；Host mid-sync | K01 已承担 `G`；K02 勿再 fused 进 Decrypt 核 |
| 2 | **`BLOCK_DIM=1`** | 禁多 AIV 拆 Â / peer |
| 3 | **flag ∈ {1,3} + 可选 4=GATE**；禁 5/7；禁 SoftSync | Reenc MIX 面短握手；AIC Wait 环禁 SyncAll |
| 4 | **禁 fork `l18` / stable 深 FSM（GATE=8 史）** | 新目录重拼；只认反卡死检查单 |
| 5 | **Host 不预喂 K/coins**（T22：设备 G 产 coins） | coins 仅来自 K01；输入 `m'+ek+coins` |
| 6 | **业务 GM 写出 = UB+DataCopy**（X12；T27 判决） | `c'`/`K`/`ŝ` 等禁 `GlobalTensor::SetValue` |
| 7 | **pack / BD₁₂ / CBD / NTT(y) 积木契约可参考** | 输入噪声域是 **y**（Encrypt），非 Decrypt 的 **u** |
| 8 | **权威**：同 `(ek,m',r')` 下 `c'≡` Encrypt/liboqs | I/O 等价，非源码同构 |
| 9 | **sync_audit 红线不得否决** | 每编码刀 JSON→本刀 `logs/` |

---

## 5. 压测（挂钩 deadlock-triage）

| 项 | 口径 |
|----|------|
| 范围 | **K04**（及主控认定的 Decaps 全链目录）关 `Q-RT-HANG` |
| 次数 | 建议 **×30**（对齐 T24：`ok=30 fail=0`） |
| 命令 | 主控：同目录反复 `bash run.sh -r npu -v Ascend910B3`（或战役当前 Soc） |
| 预算 | `KERNEL_COMPUTE_BUDGET_SEC` 默认 **180**；exit **124** = 疑似挂死（缺陷），≠性能不达标 |
| 成功 | 次次 SynchronizeStream 返回；`K` 交叉仍绿；无 BLOCKED |
| 失败分诊 | 挂死 → cannbot **`deadlock-triage`**（CrossCore Wait 无 Set / 配对 / Wait 环 SyncAll / flag 冲突）+ KB X1；未挂但 `K` 错 → 假绿三问 → X12/FO/切片；脏 `build/output` → X8 先清再复测 |
| 禁 | 并行多路 npu/sim；超时杀进程后不清会话污染（Encrypt X3） |

---

## 6. Basename 撞名检查单（Decrypt+Encrypt 多核合并）

编码 / 合并 K04 前逐项勾选：

- [ ] 同 binary 内每个 kernel `.cpp` **basename 全局唯一**（C-UNIQUE-BASENAME / X13）。  
- [ ] Decrypt 侧前缀：`dec_prep_*.cpp`、`dec_ntt_*.cpp`、`dec_intt_*.cpp`（或 S0A 锁定名）——**禁用**裸名 `prep_custom.cpp`。  
- [ ] Re-Encrypt 侧前缀：`reenc_prep_*.cpp`、`reenc_mix_*.cpp`、`reenc_pack_*.cpp` 等——**不得**与 Encaps 旧树或 Decrypt 侧同名。  
- [ ] FO / G：`decaps_g_*.cpp`、`decaps_fo_*.cpp`。  
- [ ] K04 若链入设备 Encaps 核：Encaps basename 亦须 `enc_*` 且与 `dec_*`/`reenc_*` 不撞。  
- [ ] CMake/`ascendc_*` 源列表无重复 basename；上板前若 `507000` / TRACE=0 → **先查撞名**再查算法。  
- [ ] 禁止从 `RB-T25/26/27` **复制文件名**进新树。

---

## 7. next_hint

**S0A 与 S0B 均 DESIGN_OK 回收后**：主控按 DAG `D-NEAR-FIRST-CODE` **先派 `DRW-D01`（Decrypt prep / 建议目录 `RB-D01-decrypt-prep`）**，关 DG1→…→`Q-DEC-CORRECT`；**不要**抢先开 `DRW-K01`。  
Decaps 第一编码刀 **`DRW-K01` / `RB-D04-decaps-G`** 仅在 **Decrypt NPU+liboqs 收口** 之后。

---

## 附录 · 与临时刀判决对齐（只读）

| 来源 | 可用判决 | 本计划处置 |
|------|----------|------------|
| T22 | 设备 G + 双 launch + Host 不预喂 coins | K01/K02 契约继承 |
| T24 | 往返 + **×30** 不挂压测口径 | K04 压测模板 |
| T26 | 五 launch CPU 绿；Decrypt 1/3、Reenc 1/3+4 | 仅知「可拆」；**禁抄源码** |
| T27 | X12 SetValue→K 错；DataCopy 修复 | 全战役写出铁律 |

可选草图：[`logs/queue-sketch.md`](logs/queue-sketch.md)。

## 主控批注（2026-09-09）

- **采纳**：K01 G → K02 ReEnc → K03 FO（合法+拒绝）→ K04 往返×30；Decaps 一律 `deps: Q-DEC-CORRECT`。  
- **入账**：KB §A2 接口表指针；DAG `E-S0B-QUEUE` closed；QUEUE 刀序行。  
- **下一步**：派 **DRW-D01**（Decrypt prep / CPU·subagent）；**勿**开 K01；云 NPU 未开，D01 不上板。

