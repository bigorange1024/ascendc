# 2026-07-03 — Alg.21 Decaps SIM 单 session 根因定位

## 结论

- `fix-f203-alg21-kem-decaps-k4` 单库后，CPU 单 session 合法 `c` 仍 `K max=0`。
- SIM 完整单 session（`KEM_DECAPS_SIM_2SESSION=0`）可跑完但 `K max=216`，不是死锁。
- 逐级 dump 对拍显示：`a_hat/re/t_hat/r_hat` 全部 max=0，首个出错级是 `at_r5` 输出 `u_hat/tr_hat`。
- PhaseE-only 对照（`KEM_DECAPS_PHASEE_ONLY=1`，跳过 Phase-D，复用已验证 `m'/K'/coins`）在同一 decaps binary/session 中 `K max=0`，且 `u_hat/tr_hat` max=0。

## 判读

- `at_r5` 本身、Phase-E 前级输入、host 拼装 `mat`、LUT 与 stream 同步均不是根因。
- 污染由 **Phase-D 已执行过**这一事实触发，属于 CAModel session 级状态残留；`aclFinalize` 后 fresh Phase-E（默认 2-session）可清除。
- “单工程/单设备库”不足以彻底解决；若要继续追单 session，需要对 Phase-D 子集二分，定位是哪一个 Phase-D kernel 污染后续 `at_r5`。

## 命令证据

```bash
KEM_DECAPS_FORCE_REBUILD=1 CMAKE_BUILD_JOBS=1 \
KEM_DECAPS_PHASEE_ONLY=1 KEM_DECAPS_SIM_2SESSION=0 \
KEM_DECAPS_DEBUG=1 KEM_DECAPS_VERIFY=1 \
bash run.sh -r sim -v Ascend910B4

python3 scripts/diag_phase_e.py
```

结果：`[verify_kem_decaps] K max=0`，`diag_phase_e.py` 显示 `a_hat/re/t_hat/r_hat/u_hat/tr_hat` 全部 `max_diff=0`。

## 设备 FO + SIM 2-session 默认（本日续）

- Decaps FO（`c` vs `c'` 比对 + `J(z‖c)` + 选 K）**CPU/SIM 均在 device**（`f203_kem_dec_pack::KemDecFo`）；SIM 2-session 去掉旧 host `memcmp`，改为 Phase-D `aclFinalize` 后 fresh session 跑 Phase-E + 设备 FO。
- 拒绝路径：`KEM_DECAPS_TAMPER_C=1` 在 device 改 `coins[0]`（输入 `c` 不变）→ FO 走 `J(z‖c)`。CPU `REJECT PASS`；SIM 2-session 合法路径 `K max=0`。
- 三个 KEM 探针 `run.sh` 默认改为**生产全量 + golden 对拍**（`KEM_*_VERIFY=1`、`SKIP_REBUILD=1`），用户直接 `bash run.sh -r cpu/sim` 即可，无需手动 export；keygen 补上与 encaps/decaps 一致的 RUN_MODE stamp 增量编译。

## liboqs ↔ KEM 算子正确性脚本（镜像 PKE）

仓库级 `scripts/`，两套：

| 脚本 | 作用 | CPU 证据 |
|------|------|----------|
| `liboqs_kem_ref.c`（+`decaps` 子命令）+ `build_liboqs_kem_ref.sh` | liboqs 黑盒 keygen/encaps/decaps | ref 二进制 OK |
| `liboqs_kem_fixture.py` | 一次生成 ek/dk/c/K/K_decaps/c_bad/K_reject + 自洽校验 | self-check passed |
| `liboqs_kem_vs_ascendc.sh` + `..._verify.py` | **四阶段交叉验证**：KeyGen→Encaps→Decaps(accept)→Decaps(reject)，逐级对 liboqs fixture | ek/dk/c/K/K(decaps)/K(reject) 全 max=0；agreement + reject≠accept PASS |
| `roundtrip_kem_keygen_encaps_decaps.sh` + `roundtrip_kem_verify.py` | **纯 device 闭环**：`Decaps(Encaps.c)==Encaps.K`；拒绝 `K==J(z‖c)` 且≠K_enc，全程不借 liboqs | agreement PASS、reject PASS |

- seed 派生前缀锁定：`d=SHA3-256(exp-mlkem-f203-2s1e-k4:SEED_D=…)`、`z=…kem-k4:SEED_Z=…`、`m=…kem-encaps-k4:SEED_M=…`；三探针 device 派生与 fixture 逐字一致（keygen/encaps VERIFY max=0 佐证）。
- 拒绝路径两种触发：交叉验证用 liboqs **篡改输入密文 `c_bad`**（对 `decaps(dk,c_bad)`）；闭环用 device **`KEM_DECAPS_TAMPER_C=1` 改内部 coins**（输入 `c` 不变 → `J(z‖c)`）。golden 均为 `J(z‖·)`，触发点不同。
- SIM：两脚本 `-r sim` 为一等入口；Decaps 走 2-session（~11min/段），勿并行其他 SIM。CPU 已全绿；SIM 待长测。

## kem.keygen 相同随机字节批测（旁路 A，本日续）

kem.keygen 不吃输入、只吃随机性。用户要求让 liboqs 与探针吃**相同随机字节**（`os.urandom` 64B `kem_seed=d‖z`），CPU×10 + SIM×1 对拍 ek/dk。

- **旁路 A（`KEM_KG_EXT_SEED`，test-only 宏，默认关，用户 2026-07-03 确认例外）**：宏开时 `seed_d_gm` 扩 64B 承载 `kem_seed`；prep 取 `[0:32]` 作 `d`（`BuildKeygenPrepSinglePipeExtD`，ρ‖σ=G(d‖k)），finish 取 `[32:64]` 作 `z`。与 §4.2「禁止 host 预填 kem_seed」的生产契约不冲突（仅测试例外）。
- **落点 B1**：probe-local `kem/f203_keygen_prep_entry_extseed.cpp` + `f203_keygen_prep_extseed.hpp`；CMake 宏开时替换 vendored prep 入口（符号名不变，main 不改）。**不碰 stable/vendor**，抗 `vendor_sync_from_stable_keygen.sh` 的 rsync 覆盖。
- **脚本**：`scripts/liboqs_kem_keygen_fixture.py`（os.urandom kem_seed + liboqs `keypair_derand` golden）、`scripts/liboqs_kem_keygen_batch.sh`（CPU×10+SIM×1，复用 `liboqs_kem_vs_ascendc_verify.py --stage keygen`）。
- **结果**：**11/11 PASS**，ek_kem/dk_kem 与 liboqs 逐字节 max=0。
- **根因教训（CPU 过 SIM 挂）**：宏用 `target_compile_definitions` 只对 CPU（g++ 库）生效；SIM/NPU 走 CCE 设备编译须 `ascendc_compile_definitions`，否则 kernel 退回 `#else` seed_d 分支（把随机 d 前 4B 当 seed_d）→ SIM 输出 max~250 全错。改用按 RUN_MODE 分派后 SIM PASS。

## Encaps / Decaps 分项 liboqs kat（本日续）

用户确认：**先两个独立脚本**，不复测 keygen；Decaps 用 liboqs 造密文。

| 脚本 | 固定输入 | 每轮随机 | device | 对拍 |
|------|----------|----------|--------|------|
| `liboqs_kem_encaps_batch.sh` | stash `ek_kem` | `os.urandom(32B)` → `m` | `KEM_ENC_EXT_SEED=1` Encaps | `c/K` vs liboqs |
| `liboqs_kem_decaps_batch.sh` | stash `dk_kem` | `m` → liboqs `encaps_derand(ek,m)` 得 `c` | Decaps（无随机旁路） | `K` vs liboqs |

- **stash**：`output/kem_keypair_stash/{ek_kem,dk_kem}.bin`；`kem_keypair_stash_bootstrap.sh` 从 alg19 output 复制；keygen kat 首轮 CPU 成功后亦自动写入 stash。
- **Encaps 旁路 A**：`KEM_ENC_EXT_SEED=1` 仅旁路 `m`；`G(m‖H(ek))` 仍在 device；见 alg20 INTEGRATION_PLAN §4.2.1。
- **分项结果**：KeyGen `CPU×3+SIM×1`、Encaps `CPU×10+SIM×1`、Decaps `CPU×10+SIM×1` 均 PASS；Decaps SIM 证据为 `KEM_DEC_CPU_TRIALS=0 KEM_DEC_SIM_TRIALS=1` 复跑后 `SIM 1/1 OK`。
- **坑**：kat 曾设 `EK_KEM_SRC=input/ek_kem.bin` 与 gen_data 目标同路径 → `SameFileError`；改为 `EK_KEM_SRC=stash/ek`，Decaps `C_SRC` 用临时文件；gen_data 同路径 skip copy。
- **Decaps SIM 构建坑**：`vendor/pke_encrypt/prep/a_hat/alg7/f203_alg7_rej_scalar.c` 是 CPU/参考语义文件，不参与设备热路径；若进入 `ascendc_library`，AIC/AIV 合并阶段 `ld.lld -m aicorelinux` 报 `.c.o unknown file type`。修法：CPU twin 编入该 `.c`，SIM/NPU 设备库只保留 `.cpp` kernel 入口与 `.hpp` 内联逻辑。

## KEM run.sh build profile 隔离 + keygen flaky 决策（本日续）

用户选择：**1A**（keygen flaky 单独定位，不加脚本重试掩盖） + **2B**（keygen/encaps/decaps 三探针均做 build profile 隔离）。

### profile 规则

| 探针 | profile | 触发 | 目录 |
|------|---------|------|------|
| Alg.19 KeyGen | `prod` | 默认 / round-trip（`KEM_KG_EXT_SEED=0`） | `build_prod_<mode>` / `out_prod_<mode>` |
| Alg.19 KeyGen | `extseed` | liboqs keygen kat（`KEM_KG_EXT_SEED=1`） | `build_extseed_<mode>` / `out_extseed_<mode>` |
| Alg.20 Encaps | `prod` | 默认 / round-trip（`KEM_ENC_EXT_SEED=0`） | `build_prod_<mode>` / `out_prod_<mode>` |
| Alg.20 Encaps | `extseed` | liboqs encaps kat（`KEM_ENC_EXT_SEED=1`） | `build_extseed_<mode>` / `out_extseed_<mode>` |
| Alg.21 Decaps | `prod` | 默认 / round-trip | `build_prod_<mode>` / `out_prod_<mode>` |
| Alg.21 Decaps | `kat` | liboqs decaps kat（`KEM_DECAPS_KAT=1`） | `build_kat_<mode>` / `out_kat_<mode>` |

各探针均支持 `KEM_*_BUILD_PROFILE=<name>` 显式覆盖；`-p` 仍可覆盖 install prefix。目的：不同用途物理隔离构建产物，避免 `KEM_*_EXT_SEED`/KAT 与 round-trip 共用 `build/`、`out/`，减少宏切换、旧 `.o`、install glob 造成的“打地鼠”式污染。

### CPU 验证

- `roundtrip_kem_keygen.sh -r cpu`：`profile=prod`，stash OK。
- `roundtrip_kem_encaps.sh -r cpu`：首次建 `build_prod_cpu/out_prod_cpu`，c/K stash OK；KAT CPU×1 走 `build_extseed_cpu/out_extseed_cpu` PASS；回到 round-trip prod 仍 `skip rebuild (profile=prod)`。
- `roundtrip_kem_decaps.sh -r cpu`：首次建 `build_prod_cpu/out_prod_cpu`，agreement PASS；KAT CPU×1 走 `build_kat_cpu/out_kat_cpu` PASS；回到 round-trip prod 仍 `skip rebuild (profile=prod)` 且 agreement PASS。

### keygen flaky（不掩盖）

**现象**：build profile 隔离改造过程中观察到一次 CPU `verify FAIL`、紧接复跑 PASS。差异首字节 `ek_kem[768]`，`dk_kem` 差异从 `2304=1536+768` 起——即 `dk_kem` 内嵌那份 `ek` 的第 768 字节，与 `ek_kem[768]` **同一份数据**。故错源唯一：**`t_hat` 后半（poly 2、3，`384×2=768`）**，由 Launch2 `mmad_custom`（PKE KeyGen / Alg.13 计算核）产出；`kem_finish` 只搬运。**是 PKE KeyGen 的 `t_hat`，非 KEM 尾段。**

**复现实验（本日续二）**：profile 隔离后 `prod_cpu` 做 4×「彻底重建首跑」+ 4×「同二进制复跑」= 8 次全 PASS（此前连跑 5 次亦全 PASS），**干净隔离环境无法重现**。

**污染点定位**：计算核已多次验证、SHA3 为第三方正确实现，计算不可能错 → 只能是**构建产物污染**。触发条件三者叠加：① prep entry 双文件 `f203_keygen_prep_entry.cpp`(prod) 与 `f203_keygen_prep_entry_extseed.cpp`(extseed) **定义同名符号** `f203_keygen_prep`（CMakeLists 50–54 二选一进 `KERNEL_FILES`）；② 旧结构 `build/`+`out/` 共享；③ 同目录来回切 `KEM_KG_EXT_SEED`。SIM/host link 表现为 `multiple definition`；CPU 表现为增量漏重编「半新旧」`.so` → `t_hat` 分片里某片用旧对象 → 前 2 poly 对、后 2 poly 错（与 `d/z` 无关，否则 4 poly 全错）。

**结论**：profile 隔离（`build_prod_cpu`/`build_extseed_cpu`、`out_prod_cpu`/`out_extseed_cpu` 完全分开）消掉「同目录切宏」触发条件，问题随旧结构消失，8 次未再现印证。保留为待定位项、**不加脚本重试掩盖**；若再现则先 `FORCE_REBUILD` 排污染、再同二进制批量采样，命中后沿 `src → a_hat → t_hat/ek_pke → kem_finish` 切片二分。
