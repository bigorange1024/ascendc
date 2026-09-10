# graph-tests — cannbot 重建试验场

> **用途**：一刀一目录的积木拼装 / 同步探针；**不是** `examples/` 交付树。  
> **当前主线（2026-09-09 夜）**：**Launch 压缩** → [`launch-reduce-ops/`](launch-reduce-ops/INDEX.md)（NPU-only；KeyGen→2 / Decaps→3）  
> **已收口**：Encrypt/Encaps · Decrypt/Decaps 重建门禁 · KeyGen 重建×30（launch 偏多，待压）

## 状态（2026-09-09）

| 项 | 说明 |
|----|------|
| Encrypt/Encaps | T22–T24 **收口**（launch=2，参照） |
| Decrypt/Decaps | 门禁收口；**launch=5 待压→3** |
| KeyGen | NPU×30 关闸；**PKE=3/KEM=4 待压→2** |
| **Launch 压缩** | 计划已锁；**待 NPU 开机**；禁 CPU/SIM 结案 |

## 目录

| 路径 | 含义 |
|------|------|
| [`launch-reduce-ops/`](launch-reduce-ops/INDEX.md) | **活跃**：降 launch 全套 NPU 计划 |
| [`keygen-rebuild-ops/`](keygen-rebuild-ops/INDEX.md) | KeyGen 任务/反馈（基线） |
| [`kg_related/`](kg_related/INDEX.md) | KeyGen 重写实现（`RB-K*`） |
| [`decrypt-rebuild-ops/`](decrypt-rebuild-ops/INDEX.md) | Decrypt/Decaps（已收口基线） |
| [`dec_related/`](dec_related/INDEX.md) | Decrypt 实现 |
| [`encrypt-rebuild-ops/`](encrypt-rebuild-ops/INDEX.md) | Encrypt（已收口） |
| [`toys/`](toys/INDEX.md) | 握手玩具 |
| [`bricks/`](bricks/INDEX.md) | 编解码积木 |
| [`enc_related/`](enc_related/INDEX.md) | Encaps / Decaps 拼装 |

## 禁令

- 禁止从 `*keygen*` 算子树 / `pass-fix-f203-alg13|19*keygen*` / frozen KeyGen **抄实现**。  
- 禁止 subagent 改 KB / DAG yaml。
