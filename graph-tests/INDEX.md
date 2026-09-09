# graph-tests — cannbot 重建试验场

> **用途**：一刀一目录的积木拼装 / 同步探针；**不是** `examples/` 交付树。  
> **当前主线（2026-09-09）**：PKE/KEM **KeyGen** 重写 → [`keygen-rebuild-ops/`](keygen-rebuild-ops/INDEX.md)  
> **已收口**：Encrypt/Encaps · Decrypt/Decaps（只读继承）

## 状态（2026-09-09）

| 项 | 说明 |
|----|------|
| Encrypt/Encaps | T22–T24 **收口** |
| Decrypt/Decaps | 门禁 **收口**；`dec_related/RB-D*` 只读 |
| **KeyGen** | 脚手架 + S0 拓扑锁定；下一刀 **KGR-P01** → `kg_related/RB-K*` |

## 目录

| 路径 | 含义 |
|------|------|
| [`keygen-rebuild-ops/`](keygen-rebuild-ops/INDEX.md) | **KeyGen 任务/反馈**（活跃） |
| [`kg_related/`](kg_related/INDEX.md) | KeyGen 重写实现（`RB-K*`） |
| [`decrypt-rebuild-ops/`](decrypt-rebuild-ops/INDEX.md) | Decrypt/Decaps（已收口） |
| [`dec_related/`](dec_related/INDEX.md) | Decrypt 实现（只读） |
| [`encrypt-rebuild-ops/`](encrypt-rebuild-ops/INDEX.md) | Encrypt（已收口） |
| [`toys/`](toys/INDEX.md) | 握手玩具 |
| [`bricks/`](bricks/INDEX.md) | 编解码积木 |
| [`enc_related/`](enc_related/INDEX.md) | Encaps 拼装 |

## 禁令

- 禁止从 `*keygen*` 算子树 / `pass-fix-f203-alg13|19*keygen*` / frozen KeyGen **抄实现**。  
- 禁止 subagent 改 KB / DAG yaml。
