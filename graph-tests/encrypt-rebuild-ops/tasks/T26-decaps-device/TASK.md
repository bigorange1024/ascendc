# T26 — 设备 Decaps（Alg.21）反卡死重建

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T26` → `Q-RT-HANG` |
| 代码目录 | `graph-tests/enc_related/RB-T26-decaps-device/` |
| 运营目录 | `graph-tests/encrypt-rebuild-ops/tasks/T26-decaps-device/` |
| 墙钟 | ≤ 150 min CPU |

继承 COMMON；**NPU 优先**；SIM skip 可交。

## 背景

用户卡死点是 **Encaps↔Decaps 来回**。T25 已交设备 Decrypt；本刀做 **设备 Decaps**，复用：
- 重建 Encaps 契约：`RB-T22` / `RB-T23`（设备 G + Encrypt）— **可参考 LAYOUT/握手，禁止从 stable/alg20/alg21 抄码**
- 重建 Decrypt：`RB-T25` — **可复制同树工程壳与 Decrypt 核到本目录后编排，或 `#include` 同仓相对路径的自研头；仍禁抄 pass-fix/stable decaps**

必读：`MIX-Encrypt-Encaps-反卡死拓扑技术总结.md` §5 / §5.1。

## 目标

Alg.21 Decaps（ML-KEM-1024）设备路径：
- 输入：`dk`（或拆分 dk_pke / h / z 等按 FIPS）、`c`
- 输出：`K[32]`
- 逻辑：Decrypt→m'；再 Encaps_internal 重加密得 c'；常量时间选 K（实现上先正确性；CT 可后刀）
- **禁抄** `pass-fix-f203-alg21*`、`*decaps*`、`alg20*`、`l18_l19`、encrypt/encaps 旧树
- flag / BLOCK_DIM / SoftSync：同战役硬锁（1/3+可选4；禁 5/7）
- Golden：优先 **liboqs Decaps**；缺库 BLOCKED

## 验收

```bash
cd graph-tests/enc_related/RB-T26-decaps-device
bash run.sh -r cpu -v Ascend910B4
```

- `K` ≡ liboqs Decaps（同 dk/c）
- FEEDBACK → 主控立刻 NPU

## 下一刀预告

T27：设备 Encaps（T22/23）→ 设备 Decaps（T26）往返 + NPU 反复压测。
