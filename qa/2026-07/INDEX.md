# qa/2026-07 — 2026 年 7 月讨论索引

当月讨论摘要；细节见各日 `.md`。总表见 [../TODO.md](../TODO.md)。

---

## 讨论

| 日期 | 文件 | 关键字 |
|------|------|--------|
| 2026-07-26 | [2026-07-26-教材第7章流程图与对照图.md](2026-07-26-教材第7章流程图与对照图.md) | 第7章图 · 写作指导 · **WSL liboqs RT**：main PASS；CT CPU×5 绿 / 连续 SIM segfault / 单 Decaps SIM 绿 |
| 2026-07-25 | [2026-07-25-Decaps交付树回灌CT工程小改.md](2026-07-25-Decaps交付树回灌CT工程小改.md) | 无 `-ct` 回灌；liboqs RT；**第7章成文** |
| 2026-07-24 | [2026-07-24-第7章CT与Decaps-device-PASS.md](2026-07-24-第7章CT与Decaps-device-PASS.md) | **CT_decaps** · **`-ct` 改名** · **拒绝 SIM** · KAT **10+3** · **roundtrip** · **T22** CT tick · `M_FILE` · 未跑 NPU · 五指标 |
| 2026-07-23 | [2026-07-23-形式方法教材导论实质与第3章冻结.md](2026-07-23-形式方法教材导论实质与第3章冻结.md) | **correctness 引子** · **开放生成≠合法派生** · **证书闭包受控搜索** · 第3章**先保持**、汇报再删 · `research/formal-lang-dag` |
| 2026-07-21 | [2026-07-21-连续SIM-tcache对照矩阵.md](2026-07-21-连续SIM-tcache对照矩阵.md) | 公司 C0–C5×3 **18/18** · 家里 EXP1–4b 全绿 · **两侧未复现 tcache** · 偶发归类 · 不改脚本 |
| 2026-07-20 | [2026-07-20-Decaps交付stable与registry.md](2026-07-20-Decaps交付stable与registry.md) | **`#交付#` Decaps → stable（无 `-ct`）** · **T19i SIM 3** · **冻结 correctness×3** · **`stable_kem_liboqs_roundtrip`** · **WSL 连续 SIM `tcache` 偶发** · T13b/T11 · T23 · 幽灵清理 |
| 2026-07-18 | [2026-07-18-Decaps-pass-fix与incubating-customspec.md](2026-07-18-Decaps-pass-fix与incubating-customspec.md) | **pass-fix 更名** · **`$规格$` exp-…-kem-decaps-k4** |
| 2026-07-17 | [2026-07-17-Decaps-device-全链-PASS.md](2026-07-17-Decaps-device-全链-PASS.md) | **T19b/c 全链 + E3 PASS** · **T2 单库+1-session PASS**（D**286803**+E**745925**）· scripts→device |
| 2026-07-15 | [2026-07-15-TODO与T19a-Encaps-device-PASS.md](2026-07-15-TODO与T19a-Encaps-device-PASS.md) | **TODO** · **T19a PASS** · **Encaps → pass-fix** · **【预研】exp** · **`#验收#` → stable（721119，KAT 10+3）** · scripts 默认 stable · notes/registry · T19g · 下一 T19b/c |
| 2026-07-14 | [2026-07-14-KEM-KeyGen-incubating预研重写.md](2026-07-14-KEM-KeyGen-incubating预研重写.md) | **`#交付#` kem-keygen** · **PKE/KEM 默认哈希 RNG** · add_custom `-r/-v` · Cloud SIM 指引 |
| 2026-07-13 | [2026-07-13-thirdparty外部仓清单.md](2026-07-13-thirdparty外部仓清单.md) | **六仓** · **KEM KeyGen 删实现只留规格** · **T21** |
| 2026-07-10 | [2026-07-10-Decrypt交付stable.md](2026-07-10-Decrypt交付stable.md) | **Decrypt `#交付#`** · **统一整数 Compress/Decompress** · **§8 业界对比** · **KEM KeyGen pass-fix** · **T19a Encaps 下一** · PKE roundtrip |
| 2026-07-09 | [2026-07-09-Encrypt默认SIM_DIRECT.md](2026-07-09-Encrypt默认SIM_DIRECT.md) | **Encrypt `#交付#`** · **Decrypt 注释+I/O 收紧** · 家里续 KAT/roundtrip/T15a |
| 2026-07-08 | [2026-07-08-Alg14-tail-pack探针.md](2026-07-08-Alg14-tail-pack探针.md) | **四算子宏分层** · **compute-tail SIM 1 launch 154781**（Phase C 内联 pack）· ByteEncode 选型笔记 · tiling · sepolyvec8 CAModel FPE |
| 2026-07-07 | [2026-07-07-CPU-SIM-launch分叉约定.md](2026-07-07-CPU-SIM-launch分叉约定.md) | **prep/compute 晋级 pass-** · **compute 分平台 pass** · kP=5/v/decode SIM 完成 · CPU/SIM 分叉定案 · AGENT_HANDOFF 刷新 |
| 2026-07-06 | [2026-07-06-Encrypt-compute单launch与UB驻留.md](2026-07-06-Encrypt-compute单launch与UB驻留.md) | **3 launch PASS** · **单 launch SIM PASS** · **û UB 驻留** · SIM 标量/MTE 可见性 · FSM GATE 4/8 · CPU 单 launch 不支持 · 待 kP=5/v/decode |
| 2026-07-03 | [2026-07-03-Alg21-Decaps-SIM单session根因.md](2026-07-03-Alg21-Decaps-SIM单session根因.md) | **PhaseE-only PASS** · **首错 at_r5** · Phase-D 触发 CAModel session 残留 · SIM 默认 2-session · **设备 FO/拒绝** · **KEM run.sh 默认全量** · **liboqs_kem 四阶段 + roundtrip_kem 闭环 CPU 全绿** · **kem.keygen 旁路 A 11/11** · **Encaps/Decaps 分项 kat CPU+SIM PASS** · **KEM 三探针 build profile 隔离** · **keygen flaky 定位：错在 PKE `t_hat` 后半、疑共享 build 双 entry 混链、隔离后 8 次未再现、不加重试** |
| 2026-07-02 | [2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md](2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) | **T6 PASS** · **T7a CPU+SIM PASS** · **T7c Alg.21 有条件 PASS** · SIM 单 session c' 污染 · alg14 run.sh 待对齐 |
| 2026-07-01 | [2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md](2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md) | **liboqs PKE 三阶段** · **`Compress_5`** · **KEM Alg.19 探针 PASS** · **d/z UB** · T6 关闭 |
