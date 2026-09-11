# DRW-D04 — Decrypt 三 launch 全链拼装

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `E-D04-FULL` → `Q-DEC-CORRECT`（CPU 段） |
| 代码目录 | `graph-tests/dec_related/RB-D04-decrypt-full/`（新建） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-D04-decrypt-full/` |
| 墙钟 | ≤ 90 min |
| runner | **subagent**：拼装 + **CPU only**；**禁止** `-r npu` / SSH |

继承 [`COMMON.md`](../../COMMON.md)。

## 目标

按 S0A 拓扑在**单目录 / 单 binary** 内编排全链：

```text
Host 单 session：
  L1  ACLRT_LAUNCH(dec_prep_custom)           → sync
  L2a ACLRT_LAUNCH(dec_ntt_dot_custom)        → sync
  L2b ACLRT_LAUNCH(dec_intt_extract_custom)   → sync
  D2H m[32]
```

- 三个核文件 basename **必须保持**：`dec_prep_custom.cpp` / `dec_ntt_dot_custom.cpp` / `dec_intt_extract_custom.cpp`（可从 D01–D03 **复制源文件进本目录** 或 CMake 引用同树路径；**禁止**改成同名撞车）。  
- 输入：`dk_pke[1536]` + `c[1568]`（+ LUT）；输出：**仅** `m[32]`（生产口径）。  
- mid-sync：每次 launch 后 `aclrtSynchronizeStream`。  
- flag：L2a/L2b 各自 1/3(+4)；禁 5/7 SoftSync。  
- X12：全路径 DataCopy 写出。

## 非目标

- 不重写算法核逻辑（优先复用 D01–D03 已绿实现）。  
- 不跑 NPU；SIM 非门禁。  
- 不做 Decaps / FO。

## 必读

1. S0A FEEDBACK Launch 全表；KB §B2  
2. D01/D02/D03 STATUS + 源目录（**可复制进 RB-D04**，属本战役积木，非禁抄树）  
3. COMMON；cannbot sync_audit（对整目录跑）  
4. 若本机有 `liboqs` / `liboqs_kem_ref`：优先用其 PKE Decrypt 作 `m` 权威；否则 host FIPS 全链 oracle，FEEDBACK **必须标明**

## 禁令

- 禁抄 alg15 / T25 / examples decrypt。  
- 禁把三核融成单 launch。  
- 禁改 D01–D03 已关闭目录的「已绿」语义（若修 bug，在 FEEDBACK 说明并同步）。

## 验收（Subagent）

```bash
cd graph-tests/dec_related/RB-D04-decrypt-full
bash run.sh -r cpu -v Ascend910B4
# sync_audit → 本刀 logs/
```

| 项 | 判据 |
|----|------|
| CPU | `m ≡ oracle` max=0；三 launch TRACE/日志可证 mid-sync |
| basename | 三核文件名互不撞、且与 S0A 锁定名一致 |
| sync_audit | 无红线（假阳性写入 FEEDBACK） |

## 回报

FEEDBACK + STATUS；更新 `dec_related/INDEX.md`。  
`next_hint` 必须写：**请主控请用户开机后推 `-r npu` + liboqs**。

## 主控后续（非本刀）

用户开机 → 主控 NPU ×1… + liboqs → 关 `Q-DEC-CORRECT` → 开 K01。
