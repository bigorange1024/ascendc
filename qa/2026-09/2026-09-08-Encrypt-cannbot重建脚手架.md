# 2026-09-08 — Encrypt cannbot 重建：Encaps/Encrypt 收口

## 决策（终态）

- **目的**：旧 Encrypt/Encaps 实机卡死 → 新拓扑重拼，**NPU 不挂**为首要验收。  
- **方法**：主控 cannbot 设计 + Task subagent 编码；**禁抄**旧 encrypt/encaps/decaps/alg14/20/21。  
- **政策**：NPU 优先（910B3 `ASCEND_DEVICE_ID=0`）；长 SIM 非门禁。  
- **Q-ULT**：answered。

## 交付路径

| 角色 | 路径 |
|------|------|
| Encaps 终态用例 | `graph-tests/enc_related/RB-T22` / `RB-T23` / `RB-T24` |
| 运营 | `graph-tests/encrypt-rebuild-ops/`（QUEUE · COMMON · RHYTHM · tasks/*/FEEDBACK） |
| KB / DAG / 清单 | `docs/notes/Encrypt-cannbot-rebuild-*.md` · `docs/rg-encrypt-cannbot-rebuild.yaml` |

## 验收摘要

| 项 | 结果 |
|----|------|
| Encrypt（Alg.14，Encaps 内） | NPU 出 `c`；与 liboqs 一致（T23） |
| Encaps（设备 G + Encrypt） | T22–T24 CPU+NPU 双绿 |
| 权威交叉 | T23：`c/K` ≡ liboqs Encaps |
| 往返 | T24：liboqs Decaps → `K'≡K` |
| **不挂压测** | T24 × **30** 次 `-r npu` → **ok=30 fail=0** |

## 反卡死拓扑（成立）

双 launch（Launch1 无 CrossCore）+ flag 仅 1/3+GATE(4) + `BLOCK_DIM=1` + 禁抄旧树。详见 KB §B2。

## 教训索引

- **X11**：真机须编 aarch64 liboqs（不可搬 x86 ref）。  
- **X1/X5**：fused 单核 / fork 旧树 → 继承死锁。  
- **X6/X10**：NPU 空等杀容器。

## 未做（明确排除）

- 设备侧 Decaps（T24 Decaps 为 liboqs）。  
- `examples/stable-*` 晋级交付。

## 子 Agent（本收口相关）

- [T23 Encaps×liboqs交叉](1edad27a-a413-4a59-b736-14b24c2ed385)  
- [T24 Encaps→Decaps往返](f5eedb3b-49d7-4f90-a1df-c904e0170688)
