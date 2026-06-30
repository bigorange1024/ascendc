# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-30（晨 · Encrypt 单 session 重建 + SIM 507000 病根定位）

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 §1，**尤其 §Encrypt 与 §SIM 507000 病根**（今日核心发现） |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

---

## 1. 当前真相（2026-06-30 晨）

### Encrypt Alg.14（**汇报优先**）

**两个探针并存**——优先看新探针：

| 探针 | 角色 | 状态 |
|------|------|------|
| **新（活跃）** [`ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/`](ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/) | **单 ACL session** 重建全链（按用户要求新建目录重搭） | CPU 全链 ✅ max=0；SIM 卡在 G3 `at_r5` **507000**（病根已定位，见下） |
| 旧 [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) | 旧多 session 路线 + G4 tail 调试 | CPU 全链 ✅；SIM 多 session → 507000（已被新探针单 session 取代） |

**新探针 CPU 全链 demo（已 PASS，可汇报）**：

```bash
cd ascendc-tests/fix-f203-alg14-encrypt-2launch-k4
ENCRYPT_VERIFY=1 SIM_DIRECT=1 bash run.sh -r cpu -v Ascend910B4
# → G1/G2/G3 verify_gate max=0；[verify] PASS max=0 (1568 bytes)
```

### ⭐ SIM 507000 病根（今日最重要发现，务必传给对侧）

**结论：CAModel 单 binary 内 AIV kernel 的 `func_key ≥ 5 一律 507000；≤4 正常。**

- 全链单 session 已打通（之前多 session→507000 的病根已除）；G1/G2 SIM max=0、device decode t̂ 正常。
- G3 合并核 `f203_encrypt_at_r5`（一次出 [û|tr̂]）launch 即 **507000**，且 CAModel 同步执行→ core dump(signal 11)。
- **受控实验链（章法：逐步隔离）**：
  1. 把 G3 换成 proven `at_r`（4-poly û，`-DDIAG_G3_PROVEN_AT_R`）→ **û PASS、无 507000** ⇒ env/build/binary 正常，"MIX 后第 N 发 AIV 不可靠"假说**被推翻**。
  2. 把 `at_r5` 退化成 `kP5=4`（逐字≈at_r）→ **仍 507000** ⇒ 不是 5-poly/scratch/DataCopy 增量的锅。
  3. `nm device_aiv.o`：`f203_encrypt_at_r5_5`（**key5**）符号存在且索引一致 ⇒ 不是没编进。
  4. 对照 host_stub func_key：AIV binary = marker(0) prep_a_hat(1) prep_re(2) g4_noise(3) **at_r(4)✅** **at_r5(5)❌** g3_linear(6)❌ g3_linear4(7)❌ t_dot_r(8)❌。历史 `G3_SIM_AUDIT` 里 g3_linear(6)/g3_linear4(7) 也都 507000。**边界恰在 4↔5。**
- **func_key 不按定义序**（文件里 g3_linear 在前却拿 key6，at_r 在后拿 key4）→ 不能靠重排可靠控制。

**修复方向（家里继续做）**：把 **SIM 设备编译里 AIV kernel 总数压到 ≤5**，使合并核 `at_r5` 落到 key≤4。即把 `at_r / g3_linear / g3_linear4 / t_dot_r` 这些 SIM 不 launch 的 AIV 核 **用 `#ifdef ASCENDC_CPU_DEBUG` 仅留给 CPU**（CPU 是独立 binary、func_key 无所谓；CPU 走 `g3_linear4`），SIM 设备侧仅编 `at_r5`。同时 `main_encrypt.cpp` 旧 staged 路径（line 29/324 等引用 at_r/g3_linear/t_dot_r）需对 SIM 关掉，避免 `aclrtlaunch_*.h` 缺失编译错。
- **未验证假设**：以上修复基于"≤5 个 AIV 核 ⇒ at_r5 落 key≤4 ⇒ 不再 507000"。落地后**第一件事**：`nm` 确认 at_r5 的 key、再跑 SIM 看 507000 是否消失。若 key≤4 仍 507000，则病根另有其因，需回到受控实验。
- **时间成本提醒**：SIM 崩溃约 70s 出结果（快）；**跑通全链约 13 分钟**（G4/G5 tail 慢）。安排等待预算时注意。

**纪要**：[`qa/2026-06/`](qa/2026-06/) 当日（func_key≥5 病根）；旧审计 [`G3_SIM_AUDIT.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) §9.9。

### KeyGen（k=4，生产路径）

| 角色 | 路径 | 状态 |
|------|------|------|
| **stable 交付** | [`examples/stable/stable-mlkem-f203-pke-keygen-k4/`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | CPU/SIM/KAT ✅；SIM **542393** tick |
| **探针** | [`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | 同上（prep **双 AIV 并行 Â**） |
| **incubating 副本** | [`examples/incubating/exp-mlkem-f203-pke-keygen-k4/`](examples/incubating/exp-mlkem-f203-pke-keygen-k4/) | 保留；验收以 **stable** 为准 |
| **旧 pass（串行 Â）** | [`ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/) | **已关闭**；只读 `FROZEN.md` |

**T13h（双 AIV 并行 Â）**：✅ 完成。

**定稿**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)

### ML-KEM NTT / 2s1e 向量基线

| 路径 | 说明 |
|------|------|
| [`ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/`](ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | Alg.13 行 16–20 全链；SIM **77958** tick |
| [`ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`](ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | 8-poly NTT/INTT；NTT **30347** / INTT **30340** |

### 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4          # run.sh 内自动 SIM_DIRECT=1
bash kat_liboqs_vs_ascendc.sh             # KeyGen：CPU×10 + SIM×1
```

CPU `[SUCCESS][AIC_*]` 为 tikicpu 伪影；**以 SIM `profile_subtask_log*.toml` 为准**。

---

## 2. GitHub 同步（家里拉代码）

**推荐**：`git pull origin main` — 应包含 **Encrypt 探针整树** + G5 代码。

### 应上传（track）

| 类别 | 路径 |
|------|------|
| 探针 / 算子 | `ascendc-tests/**`（含 `fix-f203-alg14-pke-encrypt-correctness-k4/`、`frozen/`） |
| vendored LUT | `**/thirdparty/ntt_study/**` |
| 共享库 | `library/shared/**` |
| 脚本 | `scripts/**` |
| 文档 | `docs/**`、`qa/**` |
| Agent | `AGENT_HANDOFF.md`、`README.md`、`.cursor/rules/`、`.cursor/skills/` |

### 勿上传（`.gitignore`）

`backup/`、`/thirdparty/liboqs`、`**/build/`、`out/`、`input/`、`output/`、`*.bin` 等。

---

## 3. 本地备份（辅助，非主源）

```bash
bash backup-project.sh   # → backup/v0.1_YYYYMMDDHHMMSS/
```

---

## 4. 建议下一步（见 [`qa/TODO.md`](qa/TODO.md)）

| 优先级 | ID | 事项 |
|--------|-----|------|
| **P0** | **T14** | **Encrypt 新探针 SIM 全链** — 按 §SIM 507000 病根：把 SIM AIV 核压到 ≤5 使 `at_r5` 落 key≤4；先 `nm` 验 key 再跑 SIM |
| 1 | T13b | vec-k4-v3（设备 `a_hat` + V3 预采样） |
| 2 | T11 | 2s1e exp → stable 晋级 |

---

## 5. smoke（拉代码后）

```bash
# P0：Encrypt 新探针 CPU 全链（应 PASS max=0）
cd ascendc-tests/fix-f203-alg14-encrypt-2launch-k4
ENCRYPT_VERIFY=1 SIM_DIRECT=1 bash run.sh -r cpu -v Ascend910B4

# SIM 复现 507000（约 70s 崩在 at_r5）—— 修复目标
ENCRYPT_KERNEL_BUDGET_SEC=1000 ENCRYPT_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 受控实验：proven at_r 隔离（应 û PASS 无 507000；约 13 分钟跑全链）
#   在 CMake/编译期加 -DDIAG_G3_PROVEN_AT_R 即走该分支

# KeyGen
cd ../../examples/stable/stable-mlkem-f203-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
```

---

## 6. 索引

| 主题 | 链接 |
|------|------|
| Encrypt 新探针（单 session） | [`ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/`](ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/) |
| Encrypt 旧探针 STATUS | [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) |
| G3/G5 审计（func_key 病根历史） | [`G3_SIM_AUDIT.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) |
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| KeyGen 原理 | [`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) |
