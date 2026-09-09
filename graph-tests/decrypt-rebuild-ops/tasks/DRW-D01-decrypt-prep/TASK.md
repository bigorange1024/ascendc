# DRW-D01 — Decrypt L1 prep（ŝ / u / v）

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `E-D01-PREP` → `G-DG1-PREP` |
| 代码目录 | `graph-tests/dec_related/RB-D01-decrypt-prep/`（本刀新建） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-D01-decrypt-prep/` |
| 墙钟 | ≤ 60 min |
| runner | **subagent**：编码 + **CPU only**；**禁止** `-r npu` / SSH |

继承 [`COMMON.md`](../../COMMON.md)。

## 目标

实现 Decrypt 拓扑 **L1 only**（S0A 锁定）：

```text
dk_pke[1536] + c[1568]
  → ByteDecode₁₂(dk) → ŝ[k·256]
  → unpack c：Decompress₁₁ + ByteDecode → u[k·256]
               Decompress₅  + ByteDecode → v[256]
```

- 核类型：**AIV-only**，`BLOCK_DIM=1`，**零 CrossCore**。  
- 源文件 basename **必须**为 `dec_prep_custom.cpp`（全局唯一；禁 `prep_custom.cpp`）。  
- 写出：`ŝ/u/v` 经 **UB + DataCopy**（X12）；输入直读 H2D。  
- 工程壳：可对照 `graph-tests/bricks/RB-T05-bytedecode12` 的 **run.sh/CMake/main 壳**（勿整树复制改名当 Decrypt）。

## 非目标

- 不写 NTT / su_dot / INTT / extract / Decaps。  
- 不跑 SIM（除非你主动想冒烟且墙钟允许；**非门禁**）。  
- 不跑 NPU；不上云。

## 必读（只读）

1. `docs/notes/Decrypt-cannbot-rebuild-kb.md` §B2（拓扑表）、§C X12/X13  
2. `docs/rg-decrypt-cannbot-rebuild.yaml` 节点 `F-DEC-TOPO`、`E-D01-PREP`、`G-DG1-PREP`  
3. S0A FEEDBACK §1 / §7：`…/DRW-S0A-decrypt-topo-design/FEEDBACK.md`  
4. inventory DG1：`docs/notes/Decrypt-cannbot-rebuild-capability-inventory.md`  
5. `docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md`、`F203-Compress-Decompress-向量实现指南.md`（契约）  
6. `library/shared/f203_byte_codec/` 头（允许 `#include`）  
7. bricks `RB-T05` STATUS（壳参考）  
8. cannbot：本刀无 CrossCore → sync_audit 预期轻；仍建议扫一遍记入 logs

## 禁令

- 禁抄：`pass-fix-*-alg15*`、`examples/**/*decrypt*`、`RB-T25*` **源码**、`f203_decrypt_*`、frozen。  
- 禁 SoftSync / flag 5·7 / CrossCore（本刀根本不该出现）。  
- 禁 `GlobalTensor::SetValue` 写业务 GM。  
- 禁改 KB/DAG。

## 验收（Subagent）

```bash
mkdir -p graph-tests/dec_related
# 在 RB-D01-decrypt-prep 内：
bash run.sh -r cpu -v Ascend910B4
```

| 项 | 判据 |
|----|------|
| 编译 | 通过 |
| I/O | `ŝ/u/v` vs host/shared oracle **或** liboqs 中间导出（若可得）；至少 poly/缓冲级 max=0 |
| 拓扑 | AIV-only；无 CrossCore API |
| basename | 仅 `dec_prep_custom.cpp` 作核入口文件名 |
| sync_audit | 跑一遍；预期无 CrossCore 红线；JSON → 本刀 `logs/` |
| FEEDBACK | 最短格式 + STATUS 链 |

缺 liboqs 时：可用 Python/host 按 FIPS 契约造 `dk_pke/c` 与 decode golden，但须在 FEEDBACK 标明 **非 liboqs 权威**；全链权威留给后续刀。

## 回报

- `FEEDBACK.md` + `logs/`（含 sync_audit.json 若可跑）  
- 实现目录 `STATUS.md`  
- **禁止**改其它战役文件

## next_hint（给主控）

D01 CPU 绿 → 派 DRW-D02（`dec_ntt_dot_custom`）；NPU 等用户开机后由主控推 D01/全链。
