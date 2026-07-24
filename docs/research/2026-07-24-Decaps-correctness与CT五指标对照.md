# Alg.21 Decaps：correctness vs CT 路径 — 五指标对照表

> **性质**：调研记录（非交付验收）。保留精简对照口径，供教材/纪要引用。  
> **任务**：FIPS 203 Algorithm 21 `ML-KEM.Decaps`（ml_kem_1024 / k=4）  
> **精度**：A 侧假绿为 STATUS 可核对**下界**；两侧均**不比 token 倍数**。  
> **记录日**：2026-07-24

## 对照臂

| 臂 | 含义 | 主目录 |
|----|------|--------|
| **A** | correctness 史实（开放探索标本） | `ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4` |
| **B** | CT → device → `#交付#` stable | 见下「B 臂目录」 |

### B 臂目录（本轮实现）

| 角色 | 路径 |
|------|------|
| 行为基线（device） | `ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4` |
| incubating | `examples/incubating/exp-fips203-mlkem-kem-decaps-k4` |
| stable v1 | `examples/stable/stable-fips203-mlkem-kem-decaps-k4` |

## 五指标表

计分：二元项 **0/1**；假绿为**可数整数**（越低越好）。

| # | 指标 | 计分规则 | A correctness | B CT→device→stable |
|---|------|----------|---------------|---------------------|
| 1 | **先表后码** | 1=写码前有 CT 且更早提交；0=无/后补 | **0** | **1**（`999d357` CT → `a7d9286` 实现） |
| 2 | **非法是否被拦** | 1=Forbidden/锁参实际约束路径；0=开放探索未拦 | **0**（纪要：故意不给拼装路线） | **1**（禁抄 correctness/frozen；走 stable 引用） |
| 3 | **假绿/误判次数** | 声称已定位/可 PASS 后又被推翻 | **≥3**（下界：双库根因乐观→仍污染；「自旋卡死」误判；单 session「修完」再推翻） | **3**（`M_FILE`；verify 假 SUCCESS；并行污染） |
| 4 | **最终形态** | 1=可交付（无禁抄依赖，能 stable）；0=标本+vendor | **0**（`vendor/`←frozen G4/G5；oracle 标本） | **1**（device 无 vendor → `#交付#` stable） |
| 5 | **失败路径** | 1=验收含拒绝且至少双模式有据；0=仅合法 | **1**（拒绝 CPU+SIM；触发=TAMPER coins） | **1**（Gate E3；device/exp/stable 拒绝 CPU+SIM） |

## 合计

| | A | B |
|--|---|---|
| 二元项 1+2+4+5（满分 4） | **1** | **4** |
| 假绿项 3（越低越好） | **≥3** | **3** |

## 读法

- **门禁差**：B−A 在「先表 / 拦非法 / 可交付」上为 **+3**；失败路径两边都到 **1**，无差。
- **过程噪声**：假绿同量级 → CT **未消灭假绿**，只是把失败从「整路线乱飘」收成「门禁缝隙」。
- **不填**：token（两侧均无对账）。

## 依据

- A：`fix-…-decaps-correctness` `STATUS.md`；`docs/research/形式语言DAG预研讨论纪要.md` §4  
- B：`999d357` / `a7d9286`；`qa/2026-07/2026-07-24-第7章CT与Decaps-device-PASS.md`
