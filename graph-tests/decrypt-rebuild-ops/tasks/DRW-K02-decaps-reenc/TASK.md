# DRW-K02 — Decaps Re-Encrypt：ek + m' + r' → c'[1568]

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `E-K02-REENC` → `G-DG5-REENC` |
| 代码目录 | `graph-tests/dec_related/RB-D05-decaps-reenc/`（**新建**） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-K02-decaps-reenc/` |
| 墙钟 | ≤ 120 min |
| runner | **subagent**：编码 + **CPU only**；**禁止** `-r npu` / SSH |

继承 [`COMMON.md`](../../COMMON.md)。前置：**K01 / DG4 已关**（NPU 绿）。

## 目标

按 S0B / Encrypt 反卡死契约 **重拼** Re-Encrypt（I/O 等价，非源码同构）：

```text
输入：ek[1568]、m'[32]、coins=r'[32]（来自 K01 语义；本刀可独立喂 bin，禁 Host 预喂最终 c'）
建议双 launch：
  L1  AIV-only 头（ρ/切片/轻准备；无 CrossCore）→ Host mid-sync
  L2  MIX Encrypt 面：μ←m'、CBD(coins)、Â/ŷ、Mul/INTT、pack → c'[1568]
Flag L2：∈{1,3} + 可选 4=GATE；禁 5/7 SoftSync；BLOCK_DIM=1
写出：UB+DataCopy（X12）
```

- basename 前缀：`reenc_*`（如 `reenc_prep_custom.cpp` / `reenc_mix_custom.cpp`），**全局唯一**，勿与 `dec_*` / Encaps 旧 `prep_custom` 撞名。  
- 权威：同 `(ek,m',r')` 下 `c'≡` host/liboqs **PKE Encrypt**；缺库 → host FIPS Encrypt oracle，FEEDBACK 标明。  
- sync_audit 必跑。

## 非目标

- 不重做 Decrypt / G / FO。  
- 不跑 NPU。  
- **禁止** fork/对照移植：`RB-T22…T27`、alg14/20/21、examples encrypt|encaps|decaps、frozen、`l18` 深 FSM。

## 必读

1. S0B FEEDBACK §1 K02 行、§3 GM、§4 Re-Encrypt 契约表（9 条）  
2. Encrypt KB §B2（双 launch / BLOCK_DIM=1 / flag）— **只读契约**  
3. Decrypt KB · inventory DG5 · COMMON · cannbot sync_audit  
4. 可参考 toys/bricks / 编解码探针 **契约与 STATUS**，禁大段照抄核实现

## 禁令

- 禁抄 T22–T27 与 stable Encaps 源码当模板。  
- 禁把 ReEnc fused 进 Decrypt 三核。  
- 禁 Host 预喂最终 `c'`；coins 语义来自 G，勿另造随机当权威。  
- 禁改 Decrypt KB/DAG。

## 验收（Subagent）

```bash
cd graph-tests/dec_related/RB-D05-decaps-reenc
bash run.sh -r cpu -v Ascend910B4
# sync_audit → 本刀 logs/
```

| 项 | 判据 |
|----|------|
| CPU | `c'[1568] ≡` Encrypt oracle max=0 |
| 形态 | 双 launch + mid-sync；flag 合法；BLOCK_DIM=1 |
| sync_audit | 无红线 |
| INDEX | 更新 `dec_related/INDEX.md` |

## 回报

FEEDBACK + STATUS；`next_hint`：**请主控推 `-r npu`**。

## 主控后续

NPU 绿 → 派 **DRW-K03** FO（合法+拒绝）。
