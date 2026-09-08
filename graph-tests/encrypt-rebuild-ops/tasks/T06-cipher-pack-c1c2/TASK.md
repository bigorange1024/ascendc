# T06 — 密文 pack 壳 c₁‖c₂（G5）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T06` → 部分关闭 G5 |
| 代码目录 | `graph-tests/bricks/RB-T06-cipher-pack/` |
| 运营目录 | `…/tasks/T06-cipher-pack-c1c2/` |
| 墙钟 | ≤ 45 min |

继承 COMMON。

## 目标

预生成域元素 **u[k·256]、v[256]**（或已 Compress 的比特）→ **Compress₁₁(u)、Compress₅(v) → ByteEncode → c[1568]**。  
薄壳；可 Host 为主 + 可选设备；对拍 golden `c`。

## 必读

- inventory G5 · Compress/BE notes  
- 探针 `pass-f203-compress-d-vec-k4`、`pass-f203-byteencode-d-vec-k4`（d=5/11）契约

## 验收

- `c` 长度 1568；与 golden 逐字节一致（CPU；设备可选）  
- 文档写清 c₁/c₂ 偏移  
- FEEDBACK

## 依赖

无（建议 T01 已通工程壳后再做，非硬依赖）。
