# qa — 遗留事项总表

跨会话跟踪未关闭事项。刷新时须同步：**当日** `qa/YYYY-MM/YYYY-MM-DD-….md`（同日仅一篇，追加章节）+ **`qa/YYYY-MM/INDEX.md`** + **本文件**。

**最近刷新**：2026-07-13（KEM KeyGen：**删** stable+incubating 实现；**仅留** customspec；T19f 重置为「按规格重写」；T21 仍开）

---

## 里程碑：PKE 三段交付（已完成）

| 算法 | stable 算子 | 关闭项 |
|------|-------------|--------|
| Alg.13 KeyGen | [`stable-fips203-mlkem-pke-keygen-k4`](../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | T13h |
| Alg.14 Encrypt | [`stable-fips203-mlkem-pke-encrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | T14a |
| Alg.15 Decrypt | [`stable-fips203-mlkem-pke-decrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | T15a |

闭环默认：`scripts/roundtrip_pke_batch.sh` → 上述三段 stable。预研副本仍在 `examples/incubating/exp-fips203-mlkem-pke-*`。

---

## 打开项（按优先级）

主线已切 **ML-KEM（Alg.19–21）**。**阻塞交付对齐**：alg20/21 仍 vendor **frozen G5/G4**（stable 布局不兼容）→ **T19**。

| 优先级 | ID | 事项 | 状态 |
|--------|-----|------|------|
| **P0** | **T20** | **活跃用例详细中文注释补课**（跳过 `frozen/`/`add_custom/`/`vendor/`/`thirdparty/`） | **Wave1–5 完成**（2026-07-10） |
| **P0** | **T19a** | **[`fix-f203-alg20-kem-encaps-device-k4`](../ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/)**：改接 stable Encrypt / pass-fix device 布局 | **待开工**（**下一主线**） |
| **P0** | **T19b/c** | **[`fix-f203-alg21-kem-decaps-device-k4`](../ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/)** Phase-E/D | 待 T19a |
| **P0** | **T7c** | ML-KEM **Alg.21** Decaps（correctness）：[`fix-f203-alg21-kem-decaps-correctness-k4`](../ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4/) | **CPU+SIM PASS**；device：[`…-device-k4`](../ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/) 待 T19 |
| **P0** | **T7a** | ML-KEM **Alg.20** Encaps（correctness）：[`fix-f203-alg20-kem-encaps-correctness-k4`](../ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4/) | **PASS**；device：[`…-device-k4`](../ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/) 待 T19 |
| **P0** | **T6** | ML-KEM **Alg.19** KeyGen（correctness）：[`fix-f203-alg19-kem-keygen-correctness-k4`](../ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/) | **PASS**；device：[`…-device-k4`](../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) **CPU+SIM PASS** |
| **P0** | **T6f** | Alg.19 KeyGen **CPU flaky**（历史一次 FAIL/复跑 PASS；`ek_kem[768]`=`t_hat` 后半） | **隔离后 8 次未再现**；疑共享 build 混链；不加脚本重试；再现则 FORCE_REBUILD 再定位 |
| **P1** | **T21** | **调研**：能否用 [`thirdparty/SHA3hp`](../thirdparty/SHA3hp/) 把设备侧 **SHA3-256/512**（现 `library/shared/keccak_f1600_kernel` 标量）改成 AscendC 实现；范围含 KEM 尾 `H(ek)`/`z` 与 KeyGen prep `G(d‖k)` | **初步结论（2026-07-13）**：SHA3hp≠现成 SHA3-256/512；与既有 SHAKE **同系**；permute 已在用；详见当日纪要 §6；**待用户拍板** |
| **P1** | **T2** | KEM **后继**：Alg.21 **单 session SIM 真修**、**NPU 实机**（PKE/KEM） | 宜在 **T19** 后做；三分项 kat 已 PASS |
| **P1** | **T13b** | fork [`vec-k4-v2`](../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) → **vec-k4-v3**（V3 预采样 + 设备 `a_hat`） | **待开工** |
| **P2** | **T11** | **2s1e** 探针/exp → [`examples/stable/`](../examples/stable/) 晋级 | 探针 **77958** tick PASS；**stable / NPU** 未做 |
| — | **T2a** | 写 `docs/specs/fips203-mlkem1024-keygen-plan.md` | 待开工 |
| — | **T2b / T5** | KeyGen 等通用 `fips203-baseline-registry`；Encrypt 已有 [`fips203-mlkem1024-pke-encrypt-baseline-registry.md`](../docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md) | Encrypt 已补；KeyGen/Decrypt 通用表仍待 |
| — | **T12** | exp-k4 增强：liboqs 系数对照、mixPass profiling、NPU | 非阻塞 |
| — | **T3** | 他人 AscendC 代码引入流程 | 待定 |
| — | **T4** | 换机后可选重编 liboqs（OpenSSL） | 可选 |
| — | **T7** | FIPS 204 / ML-DSA | 后阶段 |
| — | **T18** | Encrypt KAT：`liboqs_pke_ref` encrypt/decrypt 链接（hidden 符号）；Decrypt 已用 `liboqs_pke_decrypt_fixture` 绕过 | 非阻塞；Encrypt KAT 若复现再修 |

### T19 — KEM device-k4 ↔ stable PKE 布局对齐（拆分）

**背景**：`*-correctness-k4` 仍 vendor 拼装（alg20/21 ← frozen G5/G4）；**实现改在 `*-device-k4`**，correctness 目录**只改名、逻辑冻结**。alg20/21 `vendor_sync` **不能** drop-in stable——见各 `FROZEN.md` 例外（仅 correctness 仍引用）。

| 子项 | 范围 | 验收 |
|------|------|------|
| **T19a** | **[`fix-f203-alg20-kem-encaps-device-k4`](../ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/)**：改接 [`stable-…-encrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)（或 pass-fix device） | CPU+SIM `c`/`K` max=0；分项 kat |
| **T19b** | **[`fix-f203-alg21-kem-decaps-device-k4`](../ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/) Phase-E**：同上 Encrypt 布局 | 与 T19a 同树；Phase-E + 全链 |
| **T19c** | **同上 device-k4 Phase-D**：改接 [`stable-…-decrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) fused | CPU+SIM `K` max=0 |
| **T19d** | **[`pass-fix-f203-alg19-kem-keygen-device-k4`](../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/)** | **PASS**（2026-07-10）；2 launch；P1 后 SIM tick 均值 **~713k** |
| **T19e** | **`scripts/` KeyGen 默认 → `pass-fix-f203-alg19-kem-keygen-device-k4`**；correctness 标为 oracle 对照 | **KeyGen 段完成**（2026-07-10）；Encaps/Decaps 脚本仍走 correctness 直至 T19a/b |
| **T19f** | 按 [`exp-…-kem-keygen-k4` customspec](../examples/incubating/exp-fips203-mlkem-kem-keygen-k4/exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf) **【预研】从零重写** incubating → 验收后再 `#交付#` stable | **进行中**：旧实现/stable **已删**（2026-07-13）；须遵守 §踩坑 SyncAll；见 AGENT_HANDOFF |

**禁止**：未改接线前把 `SRC` 指回 stable 强行 sync；从 frozen **抄码改写**冒充新实现（rsync 拼装快照除外，直至 T19e 关闭）。

### T21 — SHA3hp / 设备 SHA3-256·512 AscendC 替换（分析）

**背景**：当前设备哈希走 `library/shared/keccak_f1600_kernel`（`F203SeDeviceKeccak::Sha3OneShot` 标量）。仓库已 clone [`thirdparty/SHA3hp`](../thirdparty/SHA3hp/)。用户要求**先分析可行性**，再决定是否替换。

| 检查点 | 说明 |
|--------|------|
| API 契约 | `__aicore__` one-shot；mdlen=32（KEM 尾）与 64（prep `G`） |
| 调用点 | `kem/f203_kem_kg_*.hpp`（256）；KeyGen prep HashG（512）；Encrypt/其它若共用 |
| 验收 | 替换后 `exp-…-kem-keygen-k4` / stable PKE KeyGen CPU+SIM + golden max=0；SIM tick 对比 |
| 非目标 | 本项**仅分析**；未拍板前不改生产默认路径 |

---

### T20 — 活跃用例中文注释补课（拆分）

**决策（2026-07-10）**：1A 跳过 `vendor/`；2① 先 stable PKE + KEM，再 `pass-fix`。验收：文件头 + 函数头 + 函数体分段中文（Rule「自研代码：中文注释」）。

| 子项 | 范围 | 状态 |
|------|------|------|
| **T20-W1a** | [`stable-…-keygen-k4`](../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | **完成**（2026-07-10） |
| **T20-W1b** | [`stable-…-encrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | **完成** |
| **T20-W1c** | [`stable-…-decrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | **完成** |
| **T20-W1d** | KEM alg19/20/21 **自有**代码（跳过 vendor） | **完成** |
| **T20-W2** | `pass-fix` PKE device / prep-compute-pack 分段 | **完成** |
| **T20-W3** | NTT 积木（vec-k4-v2、stage123、innerproduct、multiplyntts、byteencode12） | **完成** |
| **T20-W4** | compress/decompress/byteencode/bytedecode、alg7/8、shake/toy 小探针 | **完成** |
| **T20-W5** | `examples/incubating/exp-*` | **完成** |

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

---

## 已关闭

| ID | 事项 | 关闭日 |
|----|------|--------|
| **T7b** | alg14 correctness `run.sh` 资源友好化 | 2026-07-10（correctness 探针已冻结；stable Encrypt 已有 SKIP_REBUILD） |
| **T15c** | 冻结 [`fix-f203-alg14/15-*-correctness-k4`](../ascendc-tests/frozen/) → 引用改 stable Encrypt/Decrypt | 2026-07-10 |
| **T15a** | Alg.15 Decrypt **`#交付#`**：[`stable-fips203-mlkem-pke-decrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) · KAT×10+1 · roundtrip×10+1 · `DECRYPT_DIR` 默认 stable · **PKE 三段齐备** | 2026-07-10 |
| **T15b** | Alg.15 Decrypt 优化探针 PASS：[`pass-fix-f203-alg15-pke-decrypt-device-k4`](../ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/) · 单 kernel + 尾融合 · SIM ~**283k** · 自 `fix-…` 改名 | 2026-07-09 |
| **T14a** | Alg.14 Encrypt **`#交付#`**：[`stable-fips203-mlkem-pke-encrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) · liboqs KAT CPU×10+SIM×1 · roundtrip CPU×10+SIM×1 · baseline-registry · 自 exp 复制晋级 | 2026-07-09 |
| **T14a-exp** | Alg.14 Encrypt **examples 预研**：[`exp-fips203-mlkem-pke-encrypt-k4`](../examples/incubating/exp-fips203-mlkem-pke-encrypt-k4/) · customspec + vendor 自包含 · I/O 仅 ek+m+coins→c · CPU+SIM `c` max=0 · SIM tick **627614** · 路线 11 LUT ROM **关闭** | 2026-07-09 |
| **T17-next** | Alg.14 **全链设备 Encrypt** PASS + 晋级：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/) · I/O 对齐 Alg.14 · CPU+SIM `c` max=0 · SIM **2 launch 626139** | 2026-07-08 |
| **T17** | Alg.14 compute+tail PASS：[`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../ascendc-tests/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) · SIM 1 launch **154781** tick | 2026-07-08 |
| **T14b** | liboqs PKE 三阶段交叉验证：[`scripts/liboqs_pke_vs_ascendc.sh`](../scripts/liboqs_pke_vs_ascendc.sh)；`SEED_D=20260619` CPU+SIM max=0；根因 **`Compress_5` `(1<<26)`** | 2026-07-01 |
| **T16** | PKE device round-trip：[`scripts/roundtrip_pke_encrypt_decrypt.sh`](../scripts/roundtrip_pke_encrypt_decrypt.sh) KeyGen→Encrypt→Decrypt；CPU+SIM **max=0** | 2026-06-30 |
| **T15** | Decrypt G4：[`frozen-fix-f203-alg15-pke-decrypt-correctness-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg15-pke-decrypt-correctness-k4/)（2026-07-10 冻结；交付继任 stable；**仍作** alg21 `vendor_sync` G4 源） | 2026-06-30 |
| **T14** | Encrypt G5：[`frozen-fix-f203-alg14-pke-encrypt-correctness-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/)（2026-07-10 冻结；交付继任 stable；**仍作** alg20/21 `vendor_sync` G5 源） | 2026-06-30 |
| **T13h** | KeyGen prep **双 AIV 并行 Â**：[`pass-fix-f203-alg13-device-keygen-k4`](../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) SIM **542339** + KAT；晋级 [`stable-fips203-mlkem-pke-keygen-k4`](../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | 2026-06-29 |
| **T13g** | Alg.13 行 3–7（16×`Â`）：[`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/) CPU+SIM ✅ | 2026-06-28 |
| **T13c** | Alg.7 单 poly SampleNTT：[`pass-fix-f203-alg7-sample-ntt-k4`](../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/) 功能完成 | 2026-06-24 |
| **T13a-v** | 设备预采样 V3：[`pass-fix-f203-alg13-lines8-15-se-k4`](../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/) PASS | 2026-06-26 |
| **T13a-c** | 链式探针 8–17 | 合入 T13a-v |
| **T13a** | 阶段一a 标量 | → [`frozen-fix-f203-alg13-se-device-scalar-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-se-device-scalar-k4/) |
| **T13i** | Phase A 全链 benchmark | → [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) |
| **T11i** | exp KeyGen 自包含交付 | 2026-06-28 |
| **T11j** | `backup-project.sh` 恢复与扩展 | 2026-06-28 |
| **T2d** | KeyGen D1 golden / liboqs KAT 布局 | 2026-06-28 |
| **T2c** | exp-mlkem1024 目录 | 由 [`exp-fips203-mlkem-pke-keygen-k4`](../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/) 承担 |
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

关闭项只追加、不删历史行。新增打开项分配简短 ID（T14 后继已用 T14a/T14b；Decrypt 交付为 T15a；liboqs_pke_ref 债为 T18）。
