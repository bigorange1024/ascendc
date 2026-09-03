# STATUS — fix-encrypt-clean-hostmu-2launch

| 项 | 状态 |
|----|------|
| 目的 | **PHASE-P0** 干净 Encrypt：Host **2-launch** + **默认 Host μ**；设备 skipNtt **无 PrefixEmbed**；AIC Wait(4) / 双 AIV SET(4) 不变量 |
| CPU | 非本线门禁 |
| SIM（TASK-007） | **PASS**：kernel wall **4.747s**；tick **22631**；magic `CLNENC01` / `out[8]=0x21`；rc=0 |
| 图谱对应 | `J-empty-trace-aic-wait4`（Wait(4)↔SET(4)）；`F-host-mu-ok-sim`（Host μ、无设备 PrefixEmbed）；`D-forbid-syncall-while-wait`；`D-next-clean-p0`；`D-reject-correctness-antipattern`（固定 2 launch） |
| Host 拓扑 | Launch-1 prep+NTT（phase=0，一轮 Cube）→ **HostFoldMuAlways**（`MU_FOLD[0]=MU01`，结构默认）→ Launch-2 skipNtt（phase=1） |
| 设备 L2 | AIC：`Wait(4)`→`Set(8)`→Cube 1/3；AIV：`StubAtJpLight` → **双 AIV SET(4)** → `Wait(8)` → Cube → magic |
| 禁令 | 无 PrefixEmbedMu / StubPrefixEmbedMu；无 SyncAll@Wait；无 SoftSync 双向；无真 SHAKE；无滥 launch；**未改** stable/Decaps/图谱 yaml |
| 结构开关 | **无** `SKEL_HOST_MU` / `SKEL_SKIPNTT`（结构即默认正确路径） |
| 日志 | `/opt/cursor/artifacts/task007_clean_hostmu_sim.log`；用例根无 stray `core*.dump`（在 `sim_log/`） |

验收（本线 SIM only）：

```bash
cd ascendc-tests/fix-encrypt-clean-hostmu-2launch
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
