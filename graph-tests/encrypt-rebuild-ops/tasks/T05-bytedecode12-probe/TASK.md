# T05 — ByteDecode₁₂ 独立终态（G2）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T05` → 关闭 `G-BD12` |
| 代码目录 | `graph-tests/bricks/RB-T05-bytedecode12/` |
| 运营目录 | `…/tasks/T05-bytedecode12-probe/` |
| 墙钟 | ≤ 40 min |

继承 COMMON。

## 目标

**ek 的 ByteEncode₁₂ 载荷 → t̂[k·256]** 独立对拍；不绑 Encrypt/KeyGen 全链 launch。  
允许 `#include` `library/shared/f203_byte_codec/` **头**并写薄壳探针；禁止从 alg14 目录搬文件。

## 必读

- inventory G2 · `F203-ByteEncode-ByteDecode-d-向量与标量选型.md`  
- 活跃 `pass-f203-alg6-bytedecode-d-vec-k4`（d≠12）作工程壳参考

## 验收

- 输入：合法 12-bit packed bytes；输出 t̂ 与 golden 一致  
- CPU +（若设备）SIM  
- FEEDBACK

## 依赖

无。
