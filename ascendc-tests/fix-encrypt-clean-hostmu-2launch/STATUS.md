# STATUS — fix-encrypt-clean-hostmu-2launch

| 项 | 状态 |
|----|------|
| 目的 | **加法重写** Encrypt：Host 2-launch + 默认 Host μ；skipNtt 无 PrefixEmbed；SET(4) 可达 |
| **P0** | ✅ SIM 绿（magic；Wait(4)/SET(4) stub） |
| **P1a** | 早 TRACE（ws+TRACE；Host dump `output/trace.bin`）；magic `out[8]=0x2A` |
| CPU | 非 hang 门禁 |
| 图谱 | `D-next-clean-p1` = P1a；见 `ENCRYPT_CLEAN_REWRITE.md` §4 |
| 禁令 | 无 PrefixEmbed；无 SyncAll@Wait；无滥 launch；禁 frozen |

验收（SIM only）：

```bash
cd ascendc-tests/fix-encrypt-clean-hostmu-2launch
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 期望：[clean-enc][P1a-trace] stages set=…；[SUCCESS] magic+TRACE OK
```

决策树：P1a 绿 → P1b（加长 at_jp stub）；红 → 缩 TRACE。
