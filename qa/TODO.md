# qa — 遗留事项总表

跨会话跟踪未关闭事项。刷新时须同步：**当日** `qa/YYYY-MM/YYYY-MM-DD-….md`（同日仅一篇，追加章节）+ **`qa/YYYY-MM/INDEX.md`** + **本文件**。

**最近刷新**：2026-07-27（**T512 W0 全绿 + W1 B4 已绿**；下一刀 B5/B6；用语：缺项/补缺）

---

## 里程碑：PKE 三段 + KEM KeyGen/Encaps/Decaps 交付（已完成）

| 算法 | stable 算子 | 关闭项 | SIM tick（登记） |
|------|-------------|--------|------------------|
| Alg.13 KeyGen | [`stable-fips203-mlkem-pke-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/) | T13h | **542393** |
| Alg.14 Encrypt | [`stable-fips203-mlkem-pke-encrypt-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) | T14a | **627590** |
| Alg.15 Decrypt | [`stable-fips203-mlkem-pke-decrypt-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) | T15a | **283290** |
| Alg.19 KEM KeyGen | [`stable-fips203-mlkem-kem-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/) | T19f | **706633** |
| Alg.20 KEM Encaps | [`stable-fips203-mlkem-kem-encaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/) | T19g | **721119** |
| Alg.21 KEM Decaps（交付） | [`stable-fips203-mlkem-kem-decaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4/) | T19h+T19i | **1041906** |
| Alg.21 KEM Decaps（CT 专题） | [`stable-fips203-mlkem-kem-decaps-ct-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-ct-k4/) | T19h-ct | D**286866**+E**763780** |

闭环默认：`scripts/roundtrip_pke_batch.sh` → PKE 三段 stable；`scripts/roundtrip_kem_keygen_encaps_decaps.sh` → KEM 设备闭环（**无 `-ct`** stable Decaps）。**办公室 KEM↔liboqs 交叉验证（推荐）**：[`scripts/stable_kem_liboqs_roundtrip.sh`](../scripts/stable_kem_liboqs_roundtrip.sh)（三件套 **stable（无 `-ct`）**；每次 `urandom`→liboqs→同字节喂 AscendC；**CPU×1+SIM×1 都绿才算数**）。KAT：`liboqs_kem_encaps_batch.sh` / `liboqs_kem_decaps_batch.sh`（默认 **无 `-ct`** stable）。**`-ct`** 树仅 CT 专题 / 第7章；勿作 scripts 默认。预研副本仍在 `examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-*`。

**07-14 已落地（非打开项）**：PKE/KEM **默认哈希 RNG**（`library/shared/fips203_host_rng/`；`SEED_D=` 仍可定点）；`add_custom` **`-r/-v`**；PKE/KEM `-r sim` **默认 `SIM_DIRECT=1`**（勿手写）。

---

## 打开项（按优先级）

主线 **ML-KEM 六算子 stable 已齐**（交付 Decaps **无 `-ct`**）；768 incubating+glue 有条件完成（禁 stable-768）。**下一方法论压力床**：**T512**（P0+P1 已落；W0 绿；W1 B4 绿）。打开项：T512 / T768-post / NPU / T23 / SHA3hp / T-WebViz。

| 优先级 | ID | 事项 | 状态 |
|--------|-----|------|------|
| — | **T-WebViz** | **方法论网页可视化**：参考领导用 DAG 网页展示研究/解决问题过程的思路，探索用网页更直观展示本方法论（依赖 DAG、门禁流、闭包演化等）；待拍板技术路线（静态 HTML / D3.js / 其它） | **低优先级，实现完成后再规划**（2026-07-27） |
| **P0** | **T6f** | Alg.19 KeyGen **CPU flaky**（历史一次 FAIL/复跑 PASS；`ek_kem[768]`=`t_hat` 后半） | **隔离后 8 次未再现**；疑共享 build 混链；不加脚本重试；再现则 FORCE_REBUILD 再定位 |
| **P0** | **T512** | ML-KEM-512：P0+P1 已落；**W0 全绿**（B1/B2/B3a/B3b）；**W1 B4 SampleNTT 2×2 已绿**；续 W1→P3 + liboqs-512 KAT/RT | **下一刀：W1 B5/B6** |
| **P0** | **T768-post** | ML-KEM-768 可选后续：device KAT 加压、D14↔D15 PKE RT、stable-768（须 `#交付#`） | **KEM liboqs 交叉 RT 已绿**（2026-07-27：`USE_LIBOQS=1` CPU×1+SIM×1）；其余仍可选；禁 stable-768 |
| **P1** | **T23** | **实验**：多 **AI Core** 并行跑 **stable 算子**（先 **2 Core**；每 Core 一份独立实例，乃至一轮 **round-trip**） | **待开工**；理论：**N 颗 AI Core ≈ N 路并行 stable**（与单算子内双 AIV 分片不同） |
| **P1** | **T21** | **调研**：能否用 [`thirdparty/SHA3hp`](../thirdparty/SHA3hp/) 把设备侧 **SHA3-256/512**（现 `library/shared/keccak_f1600_kernel` 标量）改成 AscendC 实现；范围含 KEM 尾 `H(ek)`/`z` 与 KeyGen prep `G(d‖k)` | **初步结论（2026-07-13）**：SHA3hp≠现成 SHA3-256/512；与既有 SHAKE **同系**；permute 已在用；详见当日纪要 §6；**待用户拍板** |
| **P1** | **T2-npu** | PKE/KEM **NPU 实机**验收（原 T2 中 NPU 段） | 待有卡环境 |
| — | **T2a** | 写 `docs/specs/fips203-mlkem1024-keygen-plan.md` | 待开工 |
| — | **T2b / T5** | PKE/KEM 各算子 `*-baseline-registry` | **2026-07-20 关闭**：六表齐（补 PKE KeyGen/Decrypt + Decaps）；晋级硬卡点见 ascendc-delivery Skill |
| — | **T12** | exp-k4 增强：liboqs 系数对照、mixPass profiling、NPU | 非阻塞 |
| — | **T3** | 他人 AscendC 代码引入流程 | 待定 |
| — | **T4** | 换机后可选重编 liboqs（OpenSSL） | 可选 |
| — | **T7** | FIPS 204 / ML-DSA | 后阶段 |
| — | **T18** | Encrypt KAT：`liboqs_pke_ref_mlkem1024` encrypt/decrypt 链接（hidden 符号）；Decrypt 已用 `liboqs_pke_decrypt_fixture` 绕过 | 非阻塞；Encrypt KAT 若复现再修 |

### T19 — KEM device-k4 ↔ stable PKE 布局对齐（拆分）

**背景**：`*-correctness-k4` 已冻结（vendor 拼装）；**实现改在 `*-device-k4`**。alg20/21 `vendor_sync` **不能** drop-in stable——见各 `FROZEN.md`。

| 子项 | 范围 | 验收 |
|------|------|------|
| **T19a** | **[`pass-fix-f203-alg20-kem-encaps-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/)**：改接 [`stable-…-encrypt-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) | **PASS**（CPU+SIM max=0；tick **721010**）；liboqs KAT **CPU×10+SIM×3 PASS** |
| **T19b** | **[`pass-fix-f203-alg21-kem-decaps-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/) Phase-E**（交付） | **PASS**（2026-07-17；CPU+SIM `K` max=0）；E3 可选 |
| **T19b-ct** | **[`pass-fix-…-decaps-device-ct-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-ct-k4/) Phase-E**（CT 专题） | **PASS**（第7章 CT；SIM `decaps_2session`） |
| **T19c** | **交付 device-k4 Phase-D**：stable Decrypt fused + D→E | **PASS** → **`pass-fix-…-decaps-device-k4`**（2026-07-18） |
| **T19c-ct** | **CT device-k4 Phase-D** | **PASS**（并入 T19b-ct） |
| **T19d** | **[`pass-fix-f203-alg19-kem-keygen-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4/)** | **PASS**（2026-07-10）；2 launch；P1 后 SIM tick 均值 **~713k** |
| **T19e** | **`scripts/` KeyGen/Encaps/Decaps 默认** | Encaps→stable（07-15）；**Decaps→[`stable-…-kem-decaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4/)（无 `-ct`）**（07-20 `#交付#`）；KeyGen→pass-fix device |
| **T19f** | incubating KeyGen 重写 → `#交付#` [`stable-…-kem-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/) | **完成** → 已关闭表 |
| **T19g** | incubating Encaps → `#验收#` [`stable-…-kem-encaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/) | **完成** → 已关闭表 |
| **T19h** | incubating Decaps → `#交付#` [`stable-…-kem-decaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4/)（**无 `-ct`**） | **完成**（tick **1032762**；KAT 10+3；roundtrip ✓）→ 已关闭表 |
| **T19h-ct** | CT 专题 Decaps → [`stable-…-decaps-ct-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-ct-k4/) | **完成**（KAT 10+3；roundtrip CPU+SIM；**NPU 未跑**；**非** scripts 默认） |
| **T19i** | Decaps `fo_only`→`l18_l19`（SIM 4→3；**交付树**） | **完成**（stable `#修改#` 2026-07-20） |
| **T13b / T11** | vec-k4-v3 探针包装 / 裸 2s1e stable 晋级 | **已关闭**（见已关闭表；能力已在 KeyGen stable） |

**禁止**：未改接线前把 `SRC` 指回 stable 强行 sync；从 frozen **抄码改写**冒充新实现（rsync 拼装快照除外，直至 T19e 关闭）。

### T21 — SHA3hp / 设备 SHA3-256·512 AscendC 替换（分析）

**背景**：当前设备哈希走 `library/shared/keccak_f1600_kernel`（`F203SeDeviceKeccak::Sha3OneShot` 标量）。仓库已 clone [`thirdparty/SHA3hp`](../thirdparty/SHA3hp/)。用户要求**先分析可行性**，再决定是否替换。

| 检查点 | 说明 |
|--------|------|
| API 契约 | `__aicore__` one-shot；mdlen=32（KEM 尾）与 64（prep `G`） |
| 调用点 | `kem/f203_kem_kg_*.hpp`（256）；KeyGen prep HashG（512）；Encrypt/其它若共用 |
| 验收 | 替换后 `exp-…-kem-keygen-k4` / stable PKE KeyGen CPU+SIM + golden max=0；SIM tick 对比 |
| 非目标 | 本项**仅分析**；未拍板前不改生产默认路径 |

### T23 — 多 AI Core 并行跑 stable（实验）

**目标**：验证「**一颗 AI Core = 一路独立的 stable 计算**」；有多少 Core 就有多少路并行。与单算子内部 **双 AIV 分片**（如 KeyGen prep Â）不是同一层。

| 项 | 说明 |
|----|------|
| **首刀** | **2 颗 AI Core**：各跑一份 **stable** 算子实例（同算子两份 I/O，或两路不同算子） |
| **进阶** | 每 Core 跑完整 **round-trip**（如 PKE KeyGen→Encrypt→Decrypt，或 KEM KeyGen→Encaps→Decaps） |
| **缩放** | 理论吞吐随 **AI Core 数**近似线性；须用 msprof / SIM profile 证并行占用，勿把 CPU `[SUCCESS][AIC_x]` 误读成多核（见 KeyGen 技术总结 §4.1） |
| **非目标** | 不改现有 stable 默认单实例路径；不把「算子内 AIV 并行」重标为本项 |
| **建议落点** | 新 `ascendc-tests/` 探针或 host 编排壳；复用 `examples/stable/stable-*`，**禁止**从 frozen 抄码 |
| **验收草案** | 2 路 I/O 各自与 golden/`cmp` 一致；profile 显示 2 Core 同时占用；再讨论扩到 N |

---

## 暂缓 / 待拍板（非阻塞）

| ID | 事项 | 说明 |
|----|------|------|
| **T10** | 行 18 内积 lazy reduction | 仅方案；[2026-06-12 纪要](2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md) §5–§6 |
| **T13f** | alg7 **504B+lazy tail** tick 优化 | 待用户拍板硬指标 |
| **T13d** | R5 向量 compact 独立探针 | **暂停**；生产用标量 compact |
| **T13e** | alg7 微优化（DataCopy/ROM） | 可选；期望有限 |

---

## 持续维护（非关闭）

| ID | 事项 |
|----|------|
| **T9** | [`CANN-AscendC算子开发接口参考-查阅索引.md`](../library/documents/CANN-AscendC算子开发接口参考-查阅索引.md) — 写 AscendC 前追加查阅记录 |
| **T22** | [`active_sim_regress_summary.md`](active_sim_regress_summary.md) — 活跃用例 **默认配置 SIM tick** 登记；验收通过后刷新对应行 |

---

## 已关闭

| ID | 事项 | 关闭日 |
|----|------|--------|
| **T768-W3** | ML-KEM-768 P2/W3：D19 KEM KeyGen + D20 Encaps + D21 Decaps + D21ct CT device 全部 CPU+`SIM_DIRECT=1` sim 绿；W3 device 闭环完成，下一步转 W4 incubating customspec | 2026-07-26 |
| **T768-W4** | ML-KEM-768 W4 incubating：E13–E15 PKE + E19–E21ct KEM 全部 customspec + CPU + `SIM_DIRECT=1` sim 绿；E21 tick accept **820230** / reject **822500**；E21ct tick accept **826115** / reject **825836**；未建 stable-768，下一步 registry + roundtrip | 2026-07-26 |
| **T768-glue** | ML-KEM-768 incubating 后续胶水：六份 baseline-registry 按 E13–E21ct 绿线补登记；新增 [`scripts/exp_kem768_liboqs_roundtrip.sh`](../scripts/exp_kem768_liboqs_roundtrip.sh)（当前 AscendC-only，liboqs helper 仍为 1024）；CPU×1 PASS，SIM×1 PASS，均含 accept max=0 与 reject `J(z‖c)` spot-check；未建 stable-768 | 2026-07-26 |
| **T768-W3-D21ct** | ML-KEM-768 P2/W3：D21ct KEM Decaps CT device；`dk_kem(2400)+c(1088)` → `K(32)`；CT 默认 `ASCENDC_SIM_HOST_MODE=decaps_2session`；accept CPU/SIM max=0，SIM tick **826458**（D**220868**+E**605590**）；reject CPU/SIM `K=J(z‖c)` 且 `reject≠accept`，SIM tick **823002**（D**220680**+E**602322**）；根无 stray dump；D21 delivery 默认保持 `decaps_1session` | 2026-07-26 |
| **T768-W3-D21** | ML-KEM-768 P2/W3：D21 KEM Decaps device（delivery，非 `-ct`）；`dk_kem(2400)+c(1088)` → `K(32)`；Phase-D=D15 k3 Decrypt，Phase-E=D14-shaped re-encrypt+FO；CPU+`SIM_DIRECT=1` sim 全绿，tick **818285**（D**220767**+E**597518**）；合法 `K` max=0，reject CPU/SIM `K=J(z‖c)` | 2026-07-26 |
| **T768-W3-D20** | ML-KEM-768 P2/W3：D20 KEM Encaps device；SIM 2 / CPU 5；`ek_kem(1184)+m(32)` → `c(1088)+K(32)`；CPU+`SIM_DIRECT=1` sim 全绿，tick **592129**；`c`/`K` max=0 | 2026-07-26 |
| **T768-W3-D19** | ML-KEM-768 P2/W3：D19 KEM KeyGen device；2 launch（D13 prep → compute+Alg.16 tail）；CPU+`SIM_DIRECT=1` sim 全绿，tick **510775**；`ek_kem`/`dk_kem` max=0 | 2026-07-26 |
| **T768-W2** | ML-KEM-768 P2/W2：D13 KeyGen · D14 Encrypt · D15 Decrypt device；CPU+`SIM_DIRECT=1` sim 全绿，tick D13 **373426** / D14 **507605** / D15 **222032** | 2026-07-26 |
| **T768-W1** | ML-KEM-768 P2/W1：B4 SampleNTT · B5 polyvec6 NTT/INTT · B6 multiply/inner；CPU+`SIM_DIRECT=1` sim 全绿 | 2026-07-26 |
| **T768-W0** | ML-KEM-768 P2/W0：B1 compress · B2 byteencode · B3 CBD η=2；CPU+SIM 全绿 | 2026-07-26 |
| **T2** | Decaps device SIM **单库合库 + 默认 1-session**（`prepare_dec_shim`；D**286803**+E**745925**；E3 仍绿）；NPU 实机另见 **T2-npu** | 2026-07-17 |
| **T20** | 活跃用例详细中文注释补课 Wave1–5（跳过 `frozen/`/`add_custom/`/`vendor/`/`thirdparty/`） | 2026-07-15（Wave 2026-07-10 已齐；自打开项迁出） |
| **T19f** | incubating KEM KeyGen → `#交付#` [`stable-…-kem-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/) · SIM **706633** | 2026-07-14 |
| **T19g** | incubating KEM Encaps → `#验收#` [`stable-…-kem-encaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/) · SIM **721119** · KAT **10+3** | 2026-07-15 |
| **T19i** | Decaps `fo_only`→`l18_l19`（SIM 4→3）：pass-fix + exp + stable；KAT/roundtrip ✓ | 2026-07-20 |
| **T19h** | incubating KEM Decaps → `#交付#` [`stable-…-kem-decaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4/) · SIM **1032762** · KAT **10+3** · roundtrip ✓ | 2026-07-20 |
| **T2b/T5** | PKE/KEM 六份 `*-baseline-registry` 齐；晋级硬卡点入 ascendc-delivery Skill | 2026-07-20 |
| **T13b** | fork `vec-k4-v2`→**vec-k4-v3**（V3 预采样 + 设备 `a_hat`） | 2026-07-20（**已取代**：[`stable-…-pke-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/) / KEM KeyGen 已是 prep 设备 Â+V3 + compute 2s1e；无需再维护独立 v3 探针） |
| **T11** | **2s1e** 探针/exp → `examples/stable/` 单独晋级 | 2026-07-20（**已取代**：2s1e 已随 KeyGen `compute/` 定型；不另开裸 2s1e stable） |
| **T6** | Alg.19 KeyGen correctness 路标 | 2026-07-20（**冻结** [`frozen-fix-…-alg19-…-correctness-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/)；继任 stable + pass-fix device） |
| **T7a** | Alg.20 Encaps correctness 路标 | 2026-07-20（**冻结** [`frozen-fix-…-alg20-…-correctness-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg20-kem-encaps-correctness-k4/)；继任 stable + pass-fix device） |
| **T7c** | Alg.21 Decaps correctness 路标 | 2026-07-20（**冻结** [`frozen-fix-…-alg21-…-correctness-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg21-kem-decaps-correctness-k4/)；继任 stable + pass-fix device） |
| **T7b** | alg14 correctness `run.sh` 资源友好化 | 2026-07-10（correctness 探针已冻结；stable Encrypt 已有 SKIP_REBUILD） |
| **T15c** | 冻结 [`fix-f203-alg14/15-*-correctness-k4`](../ascendc-tests/frozen/) → 引用改 stable Encrypt/Decrypt | 2026-07-10 |
| **T15a** | Alg.15 Decrypt **`#交付#`**：[`stable-fips203-mlkem-pke-decrypt-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) · KAT×10+1 · roundtrip×10+1 · `DECRYPT_DIR` 默认 stable · **PKE 三段齐备** | 2026-07-10 |
| **T15b** | Alg.15 Decrypt 优化探针 PASS：[`pass-fix-f203-alg15-pke-decrypt-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/) · 单 kernel + 尾融合 · SIM ~**283k** · 自 `fix-…` 改名 | 2026-07-09 |
| **T14a** | Alg.14 Encrypt **`#交付#`**：[`stable-fips203-mlkem-pke-encrypt-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) · liboqs KAT CPU×10+SIM×1 · roundtrip CPU×10+SIM×1 · baseline-registry · 自 exp 复制晋级 | 2026-07-09 |
| **T14a-exp** | Alg.14 Encrypt **examples 预研**：[`exp-fips203-mlkem-pke-encrypt-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4/) · customspec + vendor 自包含 · I/O 仅 ek+m+coins→c · CPU+SIM `c` max=0 · SIM tick **627614** · 路线 11 LUT ROM **关闭** | 2026-07-09 |
| **T17-next** | Alg.14 **全链设备 Encrypt** PASS + 晋级：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-pke-encrypt-device-k4/) · I/O 对齐 Alg.14 · CPU+SIM `c` max=0 · SIM **2 launch 626139** | 2026-07-08 |
| **T17** | Alg.14 compute+tail PASS：[`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) · SIM 1 launch **154781** tick | 2026-07-08 |
| **T14b** | liboqs PKE 三阶段交叉验证：[`scripts/liboqs_pke_vs_ascendc.sh`](../scripts/liboqs_pke_vs_ascendc.sh)；`SEED_D=20260619` CPU+SIM max=0；根因 **`Compress_5` `(1<<26)`** | 2026-07-01 |
| **T16** | PKE device round-trip：[`scripts/roundtrip_pke_encrypt_decrypt.sh`](../scripts/roundtrip_pke_encrypt_decrypt.sh) KeyGen→Encrypt→Decrypt；CPU+SIM **max=0** | 2026-06-30 |
| **T15** | Decrypt G4：[`frozen-fix-f203-alg15-pke-decrypt-correctness-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg15-pke-decrypt-correctness-k4/)（2026-07-10 冻结；交付继任 stable；**仍作** alg21 `vendor_sync` G4 源） | 2026-06-30 |
| **T14** | Encrypt G5：[`frozen-fix-f203-alg14-pke-encrypt-correctness-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/)（2026-07-10 冻结；交付继任 stable；**仍作** alg20/21 `vendor_sync` G5 源） | 2026-06-30 |
| **T13h** | KeyGen prep **双 AIV 并行 Â**：[`pass-fix-f203-alg13-device-keygen-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-device-keygen-k4/) SIM **542339** + KAT；晋级 [`stable-fips203-mlkem-pke-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/) | 2026-06-29 |
| **T13g** | Alg.13 行 3–7（16×`Â`）：[`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines3-7-a-hat-k4/) CPU+SIM ✅ | 2026-06-28 |
| **T13c** | Alg.7 单 poly SampleNTT：[`pass-fix-f203-alg7-sample-ntt-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg7-sample-ntt-k4/) 功能完成 | 2026-06-24 |
| **T13a-v** | 设备预采样 V3：[`pass-fix-f203-alg13-lines8-15-se-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines8-15-se-k4/) PASS | 2026-06-26 |
| **T13a-c** | 链式探针 8–17 | 合入 T13a-v |
| **T13a** | 阶段一a 标量 | → [`frozen-fix-f203-alg13-se-device-scalar-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-se-device-scalar-k4/) |
| **T13i** | Phase A 全链 benchmark | → [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) |
| **T11i** | exp KeyGen 自包含交付 | 2026-06-28 |
| **T11j** | `backup-project.sh` 恢复与扩展 | 2026-06-28 |
| **T2d** | KeyGen D1 golden / liboqs KAT 布局 | 2026-06-28 |
| **T2c** | exp-mlkem1024 目录 | 由 [`exp-fips203-mlkem-pke-keygen-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-keygen-k4/) 承担 |
| **T2e** | KeyGen D2 cpu/sim/KAT | 2026-06-29（**NPU 未测** → 并入 T2） |
| **T8** | 2026-06-09 验收口径 / NTT launch / SHA3 拍板 | 2026-06-09 |
| **T1** | Rule/Skill 与目录结构落地 | 2026-06-08 |
| — | liboqs 0.15.0 clone + 编译 + ML-KEM/ML-DSA test | 2026-06-08 |
| T11a | vec-k4-v2 阶段 A–D UB 融合；v1 冻结 | 2026-06-18 |
| T11b | vec-k4-v2 中文注释 + 编译修复 | 2026-06-18 |
| T11c | exp-k4 customspec + 预研实现 PASS | 2026-06-18 |
| T11d | byteencode12 prefetch 合入 v2（77958 tick） | 2026-06-19 |
| T11e | `f203-ntt-phase-a-fsm` 归档 frozen | 2026-06-19 |
| T11f | exp-k4 ByteEncode prefetch 同步（77996 tick） | 2026-06-19 |
| T11g | 块紧凑 S0 路线否决 | 2026-06-19 |

---

## 维护

关闭项只追加、不删历史行。新增打开项分配简短 ID（T14 后继已用 T14a/T14b；Decrypt 交付为 T15a；`liboqs_pke_ref_mlkem1024` 链接债为 T18）。
