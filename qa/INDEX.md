# qa — 讨论回忆索引

**本 INDEX 的重点**：用**关键字 + 短句**概括**每一天**的讨论，便于定位当月纪要；细节见 `qa/YYYY-MM/YYYY-MM-DD-<关键词>.md`。

遗留总表：[TODO.md](TODO.md)。当月目录：[2026-09/INDEX.md](2026-09/INDEX.md)、[2026-08/INDEX.md](2026-08/INDEX.md)、[2026-07/INDEX.md](2026-07/INDEX.md)、[2026-06/INDEX.md](2026-06/INDEX.md)、[2026-05/INDEX.md](2026-05/INDEX.md)。

正式交付文档见 `docs/`。

---

## 按时间（新→旧）

### 2026-09-04 — [PKE Enc/Dec 实机短报 · 老挂点](2026-09/2026-09-04-PKE-EncryptDecrypt实机短报老挂点.md)

关键字：**PKE Encrypt 第7轮 l18** · **Decrypt FORCE 第1轮 prod input** · 老挂点未消 · 极短回报

### 2026-09-03 — [Encaps/Decaps 真 2-launch · GATE · prep∈MIX](2026-09/2026-09-03-Encaps-Decaps真2launch与GATE.md)

关键字：**prep_ntt|l18** · **GATE 4/8** · TRACE **`0/16` 空** · Host 折 μ SIM 绿 · **Decrypt hang 单独开图** · 勿升 notes

### 2026-09-02 — [实机 l18 粘性 · TRACE 107002 · 安全拆分（当日 3-launch）](2026-09/2026-09-02-实机l18粘性与TRACE-107002.md)

关键字：**粘性≠out** · **`107002` CONTEXT_NULL** · TRACE 子线程未 SetDevice · 当日默认 prep|ntt|l18 · fused 嫌疑双 Cube · 终态见 09-03

### 2026-09-01 — [有界演示 · 合稿删 · 方法论两阶段收口](2026-09/2026-09-01-有界演示路径拓扑与合稿衔接片删除.md)

关键字：有界演示 **入目标边数锁定** · 缺项路径一次画全 · 合稿衔接片 **已删** · 对外 **两阶段（补缺/推进）** · **约束→形式语言** · **状态→自动机** · 清单→依赖差分（阶段 1 内）· 领导向三句验收

### 2026-08-31 — [方法论演示 · 开局批量底座 · 并排对比](2026-08/2026-08-31-方法论演示开局批量底座.md)

关键字：`seedProven` · **两路开局底座对齐** · **封装/解封 compare 并排** · 同节拍 · 解封约 105万/143万拍 · 对外可读文案

### 2026-08-19 — [教材第9章对照实验 · KEM 清单 · 实机测准](2026-08/2026-08-19-教材KEM清单与实机测准.md)

关键字：**第9章 A/B/C Decaps** · **表 A 14 档** · **§8 逐步指令** · **§9 Cloud Decaps 清理** · **`MSPROF_MODE=app`** · **`npu_kem_one_trip.sh`** · **kernel_details 求和** · 先 SIM 后实机

### 2026-08-18 — [领导汇报回填 · 同事向方法论标题](2026-08/2026-08-18-领导汇报回填与同事向标题.md)

关键字：CASE-002 **效果不错** · 效率/token 满意 · **底层首用例几个月超出当前方法** · 跨域样例 **ML-DSA** · 同事向讲义分轨

### 2026-08-18 — [实机 NPU 脚本套件 · 卡死恢复 · Decaps msprof 分段](2026-08/2026-08-18-实机NPU脚本套件与卡死恢复.md)

关键字：**`npu_kem_real_machine_suite.sh`** · **`npu_device_map` 分卡 stable=1 / examples=2 / tests=3** · **`npu_card_guard`/`npu_run_case`** · Decaps **D/E msprof 分段** · 无 liboqs · **alg15→stable run 对齐** · 默认 **SKIP_L18_RISK**

### 2026-08-12 — [main 拉取 · decrypt SHARED_INC 修复 · stable×7 全绿](2026-08/2026-08-12-main拉取与冒烟.md)

关键字：`SHARED_INC`/`acl_session` · stable decrypt SIM 编译修复 · **1024 stable×7 CPU+SIM PASS** · 已推 main

### 2026-08-05 — [l18_l19 卡死初步诊断 · 实机最小实验（≤4 刀）](2026-08/2026-08-05-l18卡死初步诊断与实机最小实验.md)

关键字：Encaps/Decaps 卡在 **`f203_encrypt_l18_l19` SynchronizeStream** · 收束 **MIX CrossCore**（非 pack/FO）· 假设序 **污染→GATE 4/8→INTT 1/3→NTT** · **`F203_L18_TRACE` 槽位读法** · 禁止未读 trace 改 FSM · **附录**：他仓 Cloud 纯装 CANN 通过；**禁持久 shell source set_env.sh**（已写入 Cursor-Cloud 说明）

### 2026-08-03 — [实机 device1 上 l18_l19 复跑死锁 · 缺省 ASCEND_DEVICE_ID 改回 0](2026-08/2026-08-03-实机device1-l18复跑死锁.md)

关键字：stable/探针卡在 **`l18_l19` SynchronizeStream** · **订正**：device0 能跑≠device1 坏，是**同卡脏退污染** · 前置 alg19 错 + alg20/21 卡死 · **`DeviceGuard` + LUT 硬失败 + `F203_L18_TRACE`**（Cloud 按纪要 §4 接手）

### 2026-07-31 — [借入 NPU 机体检回填 · 910B4 / CANN 9.1.0-beta.3](2026-07/2026-07-31-借入NPU机体检回填.md)

关键字：**aarch64** · cann→**9.1.0-beta.3** · toolkit/latest≡cann · **CANN_HOME 回写** · add_custom/toy-mix/**1024 探针×7**/**stable×7** 上板 env · `ASCEND_DEVICE_ID` 缺省曾为 1（**08-03 改回 0**）· **`scripts/msprof_run.sh`**（默认不采集）· **零 thirdparty 可跑**：Encrypt LUT 三级候选 + **`library/shared/f203_kem_ref`** · 软链 `ln -sfnr` 相对化 · WSL 14 用例 CPU×2 轮 + SIM 全绿

### 2026-07-30 — [实机 NPU 环境排查方案 · `add_custom` 上板适配清单](2026-07/2026-07-30-实机NPU环境排查与add_custom适配清单.md)

关键字：**借用实机只读体检（不装/不改/不 sudo）** · `find set_env.sh` + 反推 `CANN_HOME` · **`scripts/env.sh` 写死 `~/Ascend/cann` = 唯一上机即挂点** · `main.cpp` `deviceId=0` · `verify-cann.sh` 非 WSL 须降 WARN · **本轮仅落文档不改码** · T2-npu-env

### 2026-07-29 — [汇报采用 CASE-002 v4 · Cloud 稿差距复盘](2026-07/2026-07-29-汇报采用CASE002-v4与对照页版式.md)

关键字：**采用 v4** · Cloud 合稿=工程复盘 vs v4=领导短讲 · 对照页条形图缺口 · **已回填：领导会效果不错**

### 2026-07-28 — [T18 PKE ref 链接修复 · Cloud KEM RT](2026-07/2026-07-28-T18-PKE-ref链接修复.md)

关键字：**T18 关闭** · `indcpa_enc/dec` hidden 非改名 · `ml_kem_1024_ref` `.o`+fips202+`internal.a` · Cloud 三组 KEM RT 全绿

### 2026-07-27 — [768 收尾复盘 · 512 W0–W4+glue · 文档刷新](2026-07/2026-07-27-768收尾复盘与文档刷新.md)

关键字：**768/512 有条件完成** · **glue-c** · **额度复盘** · **Rule/Skill 分层落地** · **换 Agent/HANDOFF** · **用语清扫** · `$规格$`+`【预研】`

### 2026-07-26 — [教材第7章 · 1024 迁移 · 768 W0–W4](2026-07/2026-07-26-教材第7章流程图与对照图.md)

关键字：第7章 TikZ · **ml-kem-1024 迁移** · **768 P0→W4 + glue** · 幽灵 none · WSL RT/P0 main+CT 均绿（偶发仍记宿主机）

### 2026-07-25 — [Decaps 交付树回灌 CT 工程小改](2026-07/2026-07-25-Decaps交付树回灌CT工程小改.md)

关键字：`Decaps` · 无 `-ct` 回灌 · liboqs RT · **第7章成文** · **去翻译腔**

### 2026-07-24 — [第7章 CT → Decaps device PASS · 非 NPU 压测收尾](2026-07/2026-07-24-第7章CT与Decaps-device-PASS.md)

关键字：`CT_decaps` · **T19b/c** · **`-ct` 改名**避 main · **拒绝 SIM** · KAT **CPU×10+SIM×3** · **roundtrip CPU+SIM** · `M_FILE` 修复 · 未跑 NPU · 引用审计 · 中文注释 · 五指标对照 · **T22** CT 三树 tick 入 [`active_sim_regress_summary.md`](active_sim_regress_summary.md)

### 2026-07-23 — [形式方法教材导论实质 · 第3章冻结](2026-07/2026-07-23-形式方法教材导论实质与第3章冻结.md)

关键字：`correctness 引子` · **开放生成≠合法派生** · **证书闭包受控搜索** · 第3章**先保持**、汇报再删 · `research/formal-lang-dag`

### 2026-07-21 — [连续 SIM tcache 对照矩阵](2026-07/2026-07-21-连续SIM-tcache对照矩阵.md)

关键字：公司 C0–C5×3 **18/18 绿** · 家里 EXP1–4b 全绿 · **两侧均未复现 tcache** · 归类 CAModel/glibc 偶发 · **不改** roundtrip/stable

### 2026-07-20 — [Decaps `#交付#` stable · registry · 六算子齐](2026-07/2026-07-20-Decaps交付stable与registry.md)

关键字：**【预研】**→**`#交付#` stable Decaps（无 `-ct`）** · **T19i SIM 3 全链齐** · **冻结 KEM correctness×3** · **`stable_kem_liboqs_roundtrip`** · **WSL 连续 SIM `tcache` 偶发** · **T13b/T11 关闭** · **挂账 T23** · **幽灵清理**

### 2026-07-18 — [Decaps pass-fix · incubating customspec](2026-07/2026-07-18-Decaps-pass-fix与incubating-customspec.md)

关键字：`probe` → **`pass-fix-…-decaps-device-k4`** · **`$规格$`** [`exp-…-kem-decaps-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-k4/) customspec · 下一【预研】

### 2026-07-17 — [Decaps device 全链 PASS · T2 单库](2026-07/2026-07-17-Decaps-device-全链-PASS.md)

关键字：`INTEGRATION_PLAN` 先E后D · **T19b/c 全链 + E3 PASS** · **T2 单库+1-session PASS**（D**286k**+E**745k**）· `prepare_dec_shim` · scripts Decaps→device

### 2026-07-15 — [TODO · T19a · Encaps `#验收#` → stable · 能力DAG](2026-07/2026-07-15-TODO与T19a-Encaps-device-PASS.md)

关键字：`TODO` · **T20 关闭** · [`active_sim_regress_summary.md`](active_sim_regress_summary.md) · **T19a PASS**（tick **721010**）· **Encaps → `pass-fix-…`** · scripts Encaps 默认 · **`$写规格$` +【预研】exp Encaps CPU/SIM PASS**（tick≈**721k**）· **`docs/research/` 恢复** · **已验证能力 DAG 预研方法论 TeX/PDF** · 下一 **T19b/c** 或 Encaps `#交付#`

### 2026-07-14 — [KEM/PKE 默认哈希 RNG · `#交付#` · add_custom `-r/-v`](2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md)

关键字：`fips203_host_rng` · **PKE/KEM 默认 SHA3/SHAKE**（`SEED_D=` 仍可定点）· KEM KeyGen `#交付#` · add_custom **`-r/-v`** · `runtime_env` · Cloud 二次绿

### 2026-07-13 — [thirdparty · KEM KeyGen 规格重置 · T21](2026-07/2026-07-13-thirdparty外部仓清单.md)

**六仓 URL** · **debug：dk@1152 / SyncAll** · **删 stable+exp 实现，仅留 customspec** · **T21** · **AGENT_HANDOFF**

### 2026-07-10 — [Decrypt `#交付#` → stable · 统一整数 Compress/Decompress · KEM KeyGen pass-fix](2026-07/2026-07-10-Decrypt交付stable.md)

**Decrypt `#交付#`** · PKE roundtrip×10+1 · **统一整数向量 Compress/Decompress 验收** · **Compress §8 业界对比定稿** · **Alg.19 KeyGen → pass-fix** · **下一：T19a Encaps device**

### 2026-07-09 — [Encrypt 默认 SIM_DIRECT + 工程债 1–3 + 路线 11 关闭 + Encrypt `#交付#`](2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md)

**默认 `SIM_DIRECT=1`** · **Encrypt `#交付#` → stable** · **Decrypt exp 注释+Alg.15 I/O 收紧**（tick **283290**）· 家里续 KAT/roundtrip/`#交付#`

### 2026-07-08 — [Compress/ByteEncode 扩档、tail pack PASS 晋级、内核超时口径](2026-07/2026-07-08-Alg14-tail-pack探针.md)

**compress/decompress/byteencode/bytedecode *-d-vec-k4** · 四算子宏分层定稿 · **compute-tail SIM 1 launch 154781**（Phase C 内联 pack）· 笔记 ByteEncode 选型 · tail pack · tiling · sepolyvec8 CAModel FPE

### 2026-07-07 — [全仓 ASCENDC_SIM_HOST_MODE 强制 + decaps 接线](2026-07/2026-07-07-CPU-SIM-launch分叉约定.md)

**分叉指南 §4.1** · decaps `decaps_2session` · 废弃 `KEM_DECAPS_SIM_2SESSION` · `library/shared/INDEX` 选项表

### 2026-07-07 — [Encrypt prep/compute 晋级 pass- 与 CPU/SIM 分叉](2026-07/2026-07-07-CPU-SIM-launch分叉约定.md)

**全仓宏 `ASCENDC_BUILD_*` + `ASCENDC_SIM_HOST_MODE`** · 废弃 per-probe `F203_FEAS_*` · decaps 迁 `decaps_2session`

### 2026-07-06 — [Alg.14 Encrypt compute 行 18–19 单 launch 与 UB 驻留](2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md)

**3 launch CPU+SIM PASS** · **`F203_FEAS_FUSED=1` 单 launch SIM PASS**（~130s）· **û 驻留 UB** · `ProcessFromLocal` · **SIM 标量写 GM / MTE 读不可见** · FSM **GATE 4/8** · 单 launch **CPU 不支持** · 待 **kP=5 / v / decode / prep 拼接**

### 2026-07-03 — [Alg.21 Decaps SIM 单 session 根因定位 + 设备 FO + liboqs KEM 测试脚本](2026-07/2026-07-03-Alg21-Decaps-SIM单session根因.md)

**PhaseE-only PASS** · **完整单 session 首错 at_r5** · **Phase-D 触发 CAModel session 状态残留** · SIM 默认 2-session · **设备 FO/拒绝路径** · **KEM run.sh 默认全量** · **`liboqs_kem_vs_ascendc` 四阶段 + `roundtrip_kem` 闭环 CPU 全绿** · **kem.keygen 旁路 A（`KEM_KG_EXT_SEED`）相同随机字节批测 11/11** · **Encaps/Decaps 分项 kat（固定 stash；liboqs 造 c 测 decaps）CPU+SIM PASS** · **KEM 三探针 build profile 隔离** · **keygen flaky 不加重试，单独定位**

### 2026-07-02 — [KEM Alg.19 交付、Alg.20 Encaps 写码、Alg.21 Decaps 首版与 SIM 污染](2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md)

**T6 PASS** · **T7a CPU+SIM PASS** · **T7c 有条件 PASS** · **Alg.21 SIM 两段 session** · **单 session c' 污染待修**

### 2026-07-01 — [liboqs 三阶段验证、Compress_5 修复与 KEM Alg.19 KeyGen](2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md)

**liboqs_pke_vs_ascendc** · **`Compress_5` `(1<<26)`** · **frozen-fix-f203-alg19-kem-keygen-correctness-k4 规划** · 设备全链 KEM · T14b 关闭 · **T6 进行中**

### 2026-06-30 — [funckey ≥ 5 → 507000 + Encrypt G5 + Decrypt G4 + PKE round-trip](2026-06/2026-06-30-funckey-507000本地独立验证.md)

`F203_FUNCKEY_EXPERIMENT` · G5/G4 CPU+SIM max=0 · **`scripts/roundtrip_pke_*` device 闭环** · Gate/2launch 冻结

### 2026-06-29 — [KeyGen 双 AIV / Encrypt G5](2026-06/2026-06-29-KeyGen双AIV并行fork探针.md)

**fix-dual-aiv** · liboqs KAT ✅ · **Alg.14 Encrypt G5 CPU 全链 ✅** · **SIM c.bin 阻塞** · G3 审计 §9.9–§10

### 2026-06-28 — [KeyGen pass 前缀 / 子轨重命名 / Phase A 冻结 / exp 交付](2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md)

**pass-fix keygen** · **alg7/lines3-7/lines8-15 子轨重命名** · **Phase A frozen** · **exp 自包含** · **KAT** · **backup-project.sh**

### 2026-06-26 — [标量探针冻结 / KeyGen SIM prep a_hat workaround](2026-06/2026-06-26-标量探针冻结.md)

KeyGen **stable** · prep **双 AIV 并行 Â** ✅ · KAT ✅ · [AGENT_HANDOFF.md](../AGENT_HANDOFF.md) 每日交接

### 2026-06-25 — [KeyGen prep 单 TPipe / 性能优化路线图 2134](2026-06/2026-06-25-KeyGen-prep优化路线图.md)

Step4 **2 launch** · prep **454170** · Opt-2/4 ✅ · **exp-keygen-k4** 夜间批跑

### 2026-06-24 — [Alg.7 单 poly 验收 / R5 compact / 16-poly Â](2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md)

单 poly **â PASS** · R5 向量 compact **暂停** · **16-poly Â** · **KeyGen G0–G4 CPU+SIM PASS**

### 2026-06-23 — [Alg.7 SampleNTT / Phase A 向量化实验](2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md)

设备 `a_hat` Phase 1 PASS · A-v1~v4b SIM tick · **d12 POC PASS** · **rej 批量+Min+mod §13** · **d1/d2 交错 ROM Gather §14** · **shake_xof LocalTensor I/O §15** · **XOF 672B §16** · **672 vs 504 tick §17** · **rej 剔除双方案 §18** · 默认 **`F203_ALG7_REJ_IMPL=1`** · **Alg.14 Encrypt G3 fake-Â 审计+统一 g3_linear §Alg.14**

### 2026-06-19 — [ByteEncode prefetch 合入 v2 + SIM 性能复测](2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md)

prefetch 合入 vec-k4-v2 · **77958 全链路 tick** · NTT/内积/ByteEncode 单用例 SIM 表

### 2026-06-18 — [内积 a_hat 布局 & 2s1e NTT+行18 UB 融合 & exp-k4 FIPS 预研](2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md)

NTT+内积 UB 融合 · **全链路 86120 tick**（tile32 encode，6/19 被 prefetch **77958** 取代） · exp-k4 FIPS CBD CPU/SIM ✓

### 2026-06-15 — [ByteEncode₁₂ 向量、Scatter 与剩余热点](2026-06/2026-06-15-ByteEncode12向量与Scatter讨论.md)

ByteEncode₁₂ 向量探针 · 910B4 无 Scatter · pack+DataCopy · basemul 仍标量 · CPU/SIM ✓

### 2026-06-12 — [F203 Alg.13 行 18、TQue、MLKEM NTT poly-batch 定稿](2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md)

alg13 16–18 验收 · TQue/mod · **poly-batch 权威探针定稿** · 每 AIV 完整 poly · alg13 待迁移 T11

### 2026-06-11 — [engineering-notes、DataCopy、exp-int8 tiling](2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md)

engineering-notes · DataCopy 知识库定稿 · exp-int8 多核 tiling · NTT `Matmul<>` 废弃冻结 · CPU/SIM 同步

### 2026-06-10 — [F203 MIX：merged_kyber 与 limb6](2026-06/2026-06-10-F203-MIX-merged_kyber路线与limb6.md)

merged_kyber 探针 · poly2/8 batch NTT · exp-sepolyvec8-ntt-k4 CPU+SIM

### 2026-06-09 — [AscendC 平台与 CANN 文档索引](2026-06/2026-06-09-AscendC平台与CANN文档索引.md)

AscendC 9.0.0 · KernelLaunch §六 · NTT 实现备忘 §七（int8 Cube、位运算、mod 多方案未定）

### 2026-06-08 — [Rule/Skill 落地与 FIPS 203/204 终极目标](2026-06/2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md)

Rule/Skill · ML-KEM1024 KeyGen规划 · liboqs · 明日开工 · FIPS203+204

### 2026-05-19 — [目录规划与资料库结构](2026-05/2026-05-19-目录规划与资料库结构.md)

library/docs/qa 分工 · 仅根 README · 各目录 INDEX 侧重点

---

## 命名与归档（强制）

| 规则 | 说明 |
|------|------|
| **路径** | `qa/YYYY-MM/YYYY-MM-DD-<中文关键词>.md` |
| **每日一篇** | 同一自然日**只有一份**纪要；新讨论**刷新/追加**到当日文件，**禁止**同日新建第二个 `.md` |
| **标题** | 文件名与文首标题含当日讨论**关键词**（可随当日追加话题更新文件名/标题，须同步 INDEX） |
| **根目录** | `qa/` 根下仅 **`INDEX.md`**、**`TODO.md`**、可选 **[`active_sim_regress_summary.md`](active_sim_regress_summary.md)**（活跃 SIM tick）与 **`YYYY-MM/`** 月目录，**不**直接放日纪要 |

---

## 维护

- 当日有新讨论 → 编辑 `qa/YYYY-MM/YYYY-MM-DD-….md`，更新本 INDEX **一行**关键字、当月 `INDEX.md`、`TODO.md`（若有遗留变更）。
- 新日 → 在对应月目录**新建一篇**，勿复制昨日文件当模板除非有意延续结构。
