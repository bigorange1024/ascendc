ID: DESIGN_OK
cmd: review DRW-S0A
exit: 0
wall_min: 12
sync_audit: n/a
notes: Decrypt 定稿为 3 device kernel / 2 逻辑 host 段（prep AIV → mid-sync → NTT+su_dot MIX → sync → INTT+extract MIX）；flag∈{1,3,4}；禁 SoftSync/5·7；第一编码刀切 DG1。
next_hint: 开 DRW-D01（DG1 prep）：唯一 basename `dec_prep_custom.cpp`；AIV-only；I/O 对拍 ŝ/u/v vs liboqs；X12 DataCopy；禁 CrossCore。

## 主控批注（2026-09-09）

- **采纳**：L1 AIV prep → mid-sync → L2a MIX NTT+dot → sync → L2b MIX INTT+extract；flag∈{1,3,4}；basename 三分法。  
- **入账**：KB §B2 / DAG `F-DEC-TOPO`；QUEUE：S0A → **DESIGN_OK**。  
- **D01**：仍 **blocked** 至 S0B 回收（DAG `D-NEAR-FIRST-CODE` 依赖双 S0）；S0B 回后立刻派 `DRW-D01` CPU 编码（subagent）；NPU 未开机，勿空连。

---

## 1. Launch 表（Decrypt 多 launch 拓扑草案）

Host 侧：**单 ACL session** 内连续 launch；每次 `aclrtLaunchKernel` 后 **`aclrtSynchronizeStream`**（对齐反卡死 I1 + Alg.15 P-D1/P-D2）。  
全局：`BLOCK_DIM=1`；业务写出 **UB+DataCopy**（X12）；kernel `.cpp` **basename 全局唯一**（X13）。

| # | 逻辑段 | 建议核名 / **唯一 basename** | 核类型 | CrossCore | flag | 输入 GM（主） | 输出 GM（主） |
|---|--------|------------------------------|--------|-----------|------|---------------|---------------|
| **L1** | prep | `dec_prep_custom` / **`dec_prep_custom.cpp`** | **AIV-only** | **否** | — | `dk_pke`(1536)、`c`(1568)；可选 LUT | `ŝ`(polyvec k=4)、`u`(k poly)、`v`(1 poly) |
| — | mid-sync | Host `SynchronizeStream` | — | — | — | L1 写出对 Host/下一核全局可见 | — |
| **L2a** | NTT(u)+su_dot | `dec_ntt_dot_custom` / **`dec_ntt_dot_custom.cpp`** | **MIX** (AIC+AIV) | **是** | **1,3**；可选 **4=GATE**（仅本核内收口，勿与 L2b 同 launch） | `u`、`ŝ`；NTT LUT | `û`(k)、`ŵ`(1)；可选 `wPadded` |
| — | sync | Host `SynchronizeStream` | — | — | — | L2a 完成可见 | — |
| **L2b** | INTT+extract→m | `dec_intt_extract_custom` / **`dec_intt_extract_custom.cpp`** | **MIX** | **是** | **1,3**；可选 **4=GATE**（与 L2a **分核复用号**，靠 host sync 隔离争用） | `ŵ`/`wPadded`、`v` | `m`(32B) |

**示意握手（每 MIX 核独立，勿跨 launch 依赖未 sync 的 flag 状态）**：

```text
L2a / L2b 各自内部（与 Encaps 重建短表同形，禁 fork l18）：
  AIV: Set(1) → … → Wait(3) → … [→ Set(4)]
  AIC: Wait(1) → Cube… → Set(3) → [Wait(4)]
```

**规划目录（本刀不创建）**：`graph-tests/dec_related/RB-D*`；运营已在 `decrypt-rebuild-ops/`。

---

## 2. 为何拆（对照 X14 / X15 / 反卡死 I1 / C-DEC-SPLIT）

| 约束 | 本拓扑落点 |
|------|------------|
| **X14** prep∥NTT 同 launch → û 半成品 | L1 **独立 AIV launch** + Host mid-sync，再 L2a；禁止 prep 与 NTT 同 kernel |
| **X15** NTT∥INTT 同 launch 争用 flag 1–3 → m 错/死锁史 | L2a 与 L2b **两个 device kernel** + 中间 Host sync；禁止单 fused NTT+INTT |
| **I1** 跨核面要短，能拆就拆 | 无 CrossCore 的 prep 不进 MIX；Host 边界 = 全局屏障 |
| **C-ANTI-HANG** | 多 launch；flag∈{1,3,4}；`BLOCK_DIM=1`；禁 SoftSync / flag 5·7 / fork `l18` |
| **Alg.15 原理 P-D1/P-D2** | 与上表同构；**不**采用文末「生产 1-kernel+SoftSync」路径（反卡死 note §5.1 已否决） |

---

## 3. 分段验收（每 launch 后可对拍）

| Launch | 对拍张量 | Oracle | 通过判据 |
|--------|----------|--------|----------|
| **L1** | `ŝ`、`u`、`v`（布局按契约；整缓冲或 poly 级） | host/`library/shared` decode+decompress **或** liboqs 中间态导出（若有）；最终权威链仍以 liboqs PKE Decrypt 为准 | 分段 max=0；**无** CrossCore hang |
| **L2a** | `û`；`ŵ`（及 pad 后缓冲若写出） | 同 seed：NTT(u)、Σ MultiplyNTTs(ŝ,û) vs liboqs/host 参考 | `û`/`ŵ` 分段绿；监测「局部 0 / 半成品 û」（X14 复发） |
| **L2b** | `m`（32B） | **liboqs PKE Decrypt**（禁 python 冒充） | `m≡liboqs`；写出经 DataCopy（X12） |
| **全链** | `dk_pke`+`c`→`m` | liboqs | CPU 先绿；NPU×N 由主控（预算 180s） |

假绿三问（X4）：golden≠实现同源；须权威交叉；跨核 Sync 齐。

---

## 4. 风险与监测点

| 风险 | 监测 |
|------|------|
| **507000 / TRACE=0**（X13） | 同 binary 内 `dec_prep_*` / `dec_ntt_*` / `dec_intt_*` basename **互不撞名**，且不与 Encaps `prep_custom` 等同名 |
| **CPU 假绿 / NPU 错字节**（X12） | 禁止 `GlobalTensor::SetValue` 写 `m`/`ŝ`/业务 GM；H2D 直读输入 |
| **半成品 û**（X14） | L2a 前必须 mid-sync；L1 未写满勿 launch L2a；对拍 `û` 局部 0 |
| **m 错或 hang**（X15 / X1） | L2a/L2b 分核；每 Wait 路径必有 Set（SYNC-08）；AIC Wait 环禁 `SyncAll` |
| **flag 冲突** | 仅 {1,3,4}；禁 5/7；避 SyncAll 保留 [11,14]；禁 SoftSync / GATE=8 深 FSM |
| **压测脏杀**（I6） | timeout 后 Finalize/清会话；×N 同命令由主控执行 |

---

## 5. DG1–DG3 映射与第一编码刀

| Gap | Launch | 第一刀建议 |
|-----|--------|------------|
| **G-DG1-PREP** | **L1** | **DRW-D01 = 第一编码刀**：只做 prep 壳 + 分段对拍 |
| **G-DG2-NTT-DOT** | **L2a** | D01 绿后再开（依赖 mid-sync 契约） |
| **G-DG3-INTT-M** | **L2b** | DG2 绿后；全链 `m≡liboqs` 收口 Q-DEC-CORRECT |

DAG：`E-S0A-TOPO` → 激活后由主控开 `D-NEAR-FIRST-CODE` → **G-DG1-PREP**。

---

## 6. cannbot sync_audit 预防表（编码后最可能触发）

本刀无源码，不跑 `sync_audit.py`。后续 L2a/L2b 编码时优先自查：

| 条例/规则 | 预防动作 |
|-----------|----------|
| SYNC-01 Wait 先于 Set | AIC 入口勿在 AIV Set(1) 之前 Wait |
| SYNC-03/04 配对与对称 | 每 flag 1/3/(4) Set 次数 = Wait；双侧路径齐全 |
| SYNC-07 flagId | 仅用 1/3/4；勿与 Matmul 高阶 [0,2N) / SyncAll[11,14] 冲突 |
| SYNC-08 提前 return 跳过 Set | 无早退跳过 Set → 对端永久 Wait |
| SYNC-12 SyncAll | AIC CrossCore Wait **环内禁** SyncAll；prep 若需汇合仅 AIV-only 且 blockDim=1 通常可不写 |
| SoftSync / flag 5·7 | COMMON 永禁；audit 红线不得否决 |
| deadlock-triage | hang → 先查「AIV 未 Set / AIC Wait / 非法 Sync」（X1） |

L1（AIV-only）预期：**无** CrossCore；audit 侧重核内 HardEvent / 写出 DataCopy，而非 CrossCore 对。

---

## 7. next_hint — DRW-D01 TASK 草稿要点（≤10 行）

1. **目标**：新建 `graph-tests/dec_related/RB-D01-decrypt-prep/`（名可微调）；实现 **L1 prep only**。  
2. **basename**：`dec_prep_custom.cpp`（全局唯一）；**AIV-only**，`BLOCK_DIM=1`，**零** CrossCore。  
3. **功能**：ByteDecode₁₂(`dk_pke`)→`ŝ`；unpack `c`→`u,v`（d_u=11,d_v=5）；禁抄 T25/alg15 源码。  
4. **契约**：可参考 bricks BD₁₂ / compress 探针 **契约**；shared codec；X12 写出。  
5. **验收**：CPU `ŝ/u/v` vs oracle；`sync_audit`（预期无 CrossCore 红线）；**禁** `-r npu`（主控另刀）。  
6. **非目标**：不写 NTT/INTT；不建 fused decrypt。  
7. **FEEDBACK**：`DESIGN_OK` 拓扑锁定；D01 绿后派 DG2。

---

## 附录 · 与 Encaps 重建对照（只读思路）

| Encaps（T22） | Decrypt（本草案） |
|---------------|-------------------|
| L1 AIV H/G → mid-sync → L2 MIX Encrypt | L1 AIV prep → mid-sync → L2a MIX NTT+dot → sync → L2b MIX INTT+m |
| flag 1/3+4 | 同短表；**多一刀**分 NTT/INTT（X15） |
| 禁 l18 / SoftSync | 同；另禁 prep∥NTT（X14） |
