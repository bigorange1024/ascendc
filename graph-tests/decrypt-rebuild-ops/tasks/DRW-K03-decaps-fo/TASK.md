# DRW-K03 — Decaps 设备 FO（合法 + 拒绝）

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `E-K03-FO` → `G-DG6-FO` / `Q-DECAPS-CORRECT` |
| 代码目录 | `graph-tests/dec_related/RB-D06-decaps-fo/`（**新建**） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-K03-decaps-fo/` |
| 墙钟 | ≤ 90 min |
| runner | **subagent**：编码 + **CPU only**；**禁止** `-r npu` / SSH |

继承 [`COMMON.md`](../../COMMON.md)。前置：**K02 / DG5 已关**。

## 目标

设备 **FO**（本刀可独立喂 `c/c'/K'/z` 等中间量；**禁**把子进程 liboqs 当生产 Decaps）：

```text
输入（建议）：
  c[1568]、c'[1568]、K'[32]、z[32]（z←dk_kem[3136:3168) 切片语义）
设备：
  cmp ← (c ≟ c')
  若相等 → K ← K'
  否则   → K ← J(z ‖ c)   # SHAKE-256 或 FIPS 约定；与 liboqs 字节一致
写出：K[32]（UB+DataCopy，X12）
```

**必须跑两路径**（同 binary / 两次 gen 或 env 开关均可，FEEDBACK 写清）：

| 路径 | 造向量 | 期望 |
|------|--------|------|
| **合法** | 权威 Encaps/`liboqs` 得 `(c,K_ref)`；`c'=c`；`K'` 为合法 G 输出 | `K_dev ≡ liboqs_kem_decaps(dk,c) ≡ K_ref` |
| **拒绝** | 确定性篡改 `c`（如翻末字节）；`dk/z` 不变；`c'` 仍为合法重加密或保持与篡改前一致使得 `c≠c'` | `K_dev ≡ liboqs Decaps(dk,c_bad)`；且 **`K_rej ≠ K_legit`** |

- basename：`fo_*` / `decaps_fo_custom.cpp` 等，全局唯一。  
- 缺 aarch64/本机 liboqs → **BLOCKED** 权威交叉（可先 host 诊断，FEEDBACK 标明非权威）；优先编/用 `liboqs_kem_ref`。  
- sync_audit 必跑。假绿三问书面答（COMMON）。

## 非目标

- 不重做 Decrypt / G / ReEnc 全链（可 Host 造中间量）。  
- 不跑 NPU（主控可并行上板）。  
- 禁抄 T25–T27 / alg21 / examples decaps。

## 必读

1. S0B FEEDBACK §2 FO 两路径 + §3 GM（`K/c/c'/z/K'`）  
2. Decrypt KB · inventory DG6 · COMMON · X12  
3. cannbot sync_audit  

## 禁令

- 禁 python/`J` 自写冒充 liboqs 权威。  
- 禁恒输出 `K'`（拒绝路径必须不同）。  
- 禁 `SetValue` 写 `K`。  
- 禁改 Decrypt KB/DAG。

## 验收（Subagent）

```bash
cd graph-tests/dec_related/RB-D06-decaps-fo
bash run.sh -r cpu -v Ascend910B4   # 须覆盖合法+拒绝（或两次命令写清）
# sync_audit → 本刀 logs/
```

| 项 | 判据 |
|----|------|
| 合法 | `K ≡ liboqs` Decaps max=0 |
| 拒绝 | `K ≡ liboqs` Decaps(c_bad)；`≠` 合法 K |
| X12 / sync | DataCopy；audit 无红线 |

## 回报

FEEDBACK + STATUS；`next_hint`：**主控可并行 `-r npu`；绿后派 K04**。
