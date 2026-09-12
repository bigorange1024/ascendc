# AGENT_HANDOFF

**日期**：2026-09-12  
**分支**：`cursor/kem-2launch-sticky-1534`（已合入 cannbot-skills / cann-ntt-refactor / launch-reduce 等）

---

## ★ 定位

停既有 l18 debug → 新写 PKE/KEM；本侧验收/测试。  
**合 main**：收官 docs/qa（带 `__br-kem-2launch-sticky-1534`）；**scripts 不合 main**。

---


## ★ 当前 P0（2026-09-12 夜已收口）：Encrypt/Encaps **少 Host launch · SIM 全实验**

**战役**：[`graph-tests/encrypt-encaps-low-launch/`](graph-tests/encrypt-encaps-low-launch/INDEX.md) · **QUEUE 全 DONE**

| 锁定 | 结果 |
|------|------|
| AIV NTT Encrypt/Encaps **Host launch=1** | AE-E / AE-P：**cpu+SIM PASS**；c(/K)≡liboqs max=0 |
| cann-ntt Encrypt/Encaps **Host launch=2** | **EN15** / **EP06** 新建融合用例：**cpu+SIM PASS**；否决 EN13/EP04 的 8-launch 交付形态 |
| 运行态 | 仅 cpu + `SIM_DIRECT=1` sim；**未上 NPU** |

证据：`/opt/cursor/artifacts/low-launch-sim/SUMMARY.txt`。

**下一刀（待你醒）**：是否授权 commit/push；是否开 NPU sticky/perf；是否要把 Decrypt 线并进同一 launch 口径表。

**旁证（已做）**：NPU 防挂死 ×30 两线均不挂；profiling 已拉回本地——**快不等于形态正确**；本战役已按 launch 口径收口。


## ★ 旁路 P0（旧）（用户休息 · SIM-only）

**战役**：Encrypt + Encaps × **cann-ntt** · **只跑 SIM**  
**入口**：[`graph-tests/enc-encaps-cann-ntt-sim/INDEX.md`](graph-tests/enc-encaps-cann-ntt-sim/INDEX.md)  
**计划**：[`PLAN.md`](graph-tests/enc-encaps-cann-ntt-sim/PLAN.md) · **队列**：[`QUEUE.md`](graph-tests/enc-encaps-cann-ntt-sim/QUEUE.md)  
**图谱**：[`docs/rg-enc-encaps-cann-ntt-sim.yaml`](docs/rg-enc-encaps-cann-ntt-sim.yaml)（`rg_validate` OK）  
**渲染**：`/opt/cursor/artifacts/rg-enc-encaps-cann-ntt-sim.html`

| 步 | 状态 |
|----|------|
| W0a 计划+QUEUE+图谱 | **完成** |
| W0b 复跑 EN09 cpu+SIM | **完成** |
| EN13 Encrypt×liboqs SIM 交叉 | **PASS**（c max=0；SIM tick≈812698） |
| EN14 sticky R=8 | **PASS**（每轮 c max=0；SIM tick≈6455278） |
| EP01 Host 壳 / EP02 真调 Encrypt | **PASS** |
| EP03 设备哈希 | **DEFERRED_HOST** |
| EP04 Encaps×liboqs | **PASS**（c/K max=0；SIM tick≈812819） |
| EP05 Encaps sticky R=16 | **PASS**（每轮 c/K max=0；SIM tick≈12897130） |
| EN15/EP10 NPU | **BLOCKED_UNTIL_USER** |

**SIM 战役强完成**。下一步仅在你授权后上 NPU。

## ★ 旁路完成（2026-09-12）：单 AIV ML-KEM NTT 积木

**探针**：[`graph-tests/aiv_ntt/AV01-single-aiv-mlkem-ntt/`](graph-tests/aiv_ntt/AV01-single-aiv-mlkem-ntt/STATUS.md)  
**结论**：`thirdparty/ntt` **支持**单 AIV 单 poly ML-KEM；已抽取；CPU+SIM 对拍 PASS；SIM tick **7871**。  
**对照 EN01**：默认 4-poly Cube **11029**（摊销更优）；单 poly 对位 AV01 有竞争力。  
**建议**：作 **AIV NTT 基础积木**；整段替换 Encrypt×cann-ntt **暂缓**（批处理仍靠 Cube）；重写需另开战役。  
**对比日志**：`/opt/cursor/artifacts/aiv-ntt-sim-compare/`




### 硬约束（本阶段）

- **禁止 NPU / 禁止 keepalive 占卡**（用户休息，勿空转）。  
- 一刀一目录；仅 `cpu` + `SIM_DIRECT=1 sim`；禁并行多路 SIM。  
- NTT = 迁入 cann-ntt；禁抄 stable/frozen/ER 核。  
- 无授权不 commit/push/开分支。

**继承**：`enc_cann_ntt` EN01–EN12 不挂骨架已齐；本战役主攻 **正确性 + Encaps 形**。

---

## ★ 错峰（勿与 SIM 战役抢板）

**Decrypt scalar 优化**（NPU）：[`graph-tests/decrypt-scalar-opt/`](graph-tests/decrypt-scalar-opt/INDEX.md)  
H2 已强成功；H3 等用户归后再上机。休息期间 **不要** SSH 占板。

---

## ★ 非 P0

- 再采核内指令 Timeline。  
- 再压 Decrypt Host launch（已是 1）。  
- Tag5T vs cann-ntt 全链 KeyGen 换积木（另战役）。
