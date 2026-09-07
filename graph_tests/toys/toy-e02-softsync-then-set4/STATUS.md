# STATUS — toy-e02-softsync-then-set4

| 项 | 值 |
|----|----|
| task | E02 / `D-exp-e02` |
| 目录 | `graph_tests/toys/toy-e02-softsync-then-set4/` |
| 形态 | 2-launch（L1 stub + L2 SoftSyncArrive→SET(4)）× **3** 轮 |
| SoftSync | skel 单向：AIV0 写 `s[0]=1`，AIV1 `while(s[0]==0)`；再双 AIV SET(4)；AIC Wait(4) |
| 默认 SIM | **PASS**（`OMIT_SOFTSYNC=0`，TOY_ROUNDS=3） |
| 可选对照 | `OMIT_SOFTSYNC=1` 仍 **PASS** → SoftSync 对本极简骨架非必要（weaken，非 FAIL） |
| 未做 | OMIT_SET4 / 双 Cube / GATE alone（禁止复测） |

## 验收命令

```bash
cd graph_tests/toys/toy-e02-softsync-then-set4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 可选：
# TOY_ROUNDS=3 OMIT_SOFTSYNC=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 证据摘要

- Host：`100/101/110/111` 各 3；magic `E02TOY01` + `0xE2`
- L2 顺序：`500→503→502`、`510→513→512`、`400→401→402`
- kernel wall ≈ 37s / 3 轮（budget 600）
- 用例根无 stray dump
