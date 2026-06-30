# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-30（午 · Encrypt 单 session 全链 CPU+SIM 打通 max=0 ✅）

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

**两个探针并存**——以新探针为活跃基线：

| 探针 | 角色 | 状态 |
|------|------|------|
| **新（活跃）** [`ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/`](ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/) | **单 ACL session** 重建全链 | **CPU + SIM 全链 c.bin 1568B max=0 ✅** |
| 旧 [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) | 旧多 session 路线 | CPU ✅；SIM 多 session 507000（已被新探针取代） |

**验收（默认命令，无手动 export）**：

```bash
cd ascendc-tests/fix-f203-alg14-encrypt-2launch-k4
ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4                                   # CPU
ENCRYPT_KERNEL_BUDGET_SEC=1000 ENCRYPT_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # SIM ~13min
# 两者均 → G1/G2/G3 verify_gate max=0；[verify] PASS max=0 (1568 bytes)；用例根无 stray dump
```

### ⭐ SIM 单 session 两大病根 + 解法（务必传给对侧）

**病根 1：CAModel 单 binary 内 AIV kernel `func_key ≥ 5 一律 507000`，≤4 正常。**

- 受控实验（章法）：proven `at_r`(key4) PASS、`at_r5`(key5) FAIL、`at_r5` 退化 `kP5=4` 仍 FAIL、`nm` 确认符号在；
  历史 `g3_linear`(key6)/`g3_linear4`(key7) 亦 507000。边界恰在 4↔5。**KeyGen 能跑正因其恰好 5 个 AIV 核(key0-4)。**
- `func_key` 不按定义序，不能靠重排控制。
- **解法**：SIM 设备侧 `g3_linear.cpp` **只编 `at_r5`**；`g3_linear/g3_linear4/at_r/t_dot_r` 用 `#ifdef ASCENDC_CPU_DEBUG`
  仅留 CPU（CPU 独立 binary，func_key 无意义）→ SIM AIV 核 = marker/prep_a_hat/prep_re/g4_noise/at_r5 共 5 个，
  `at_r5` 落 key4。删 `main_encrypt.cpp` 旧 staged gate<5 SIM G3 + 旧 `<<<>>>` `*_do` 壳。
- **通用守则**：新增 SIM AIV 核须 `nm device_aiv.o` 复核 key≤4；AIV 核总数控制 ≤5。

**病根 2：host 拼 `matM` 前缺 `aclrtSynchronizeStream` → û 全 0。**

- `at_r5` 用 host 读回 `aHatDev/tHatDev` 拼 5×4 矩阵；D2H 前未同步 → 异步 launch 未完成 → matM 的 Â 列取 0
  → û=Σ0·r̂=0。proven `at_r` 直接在设备读 aHatDev 故无此问题（曾误导为「2nd AIV 不可靠」）。
- **解法**：host 打包 D2H 前 `aclrtSynchronizeStream(stream)`。
- **通用守则**：任何 host↔device 往返打包前必同步 stream。

**纪要**：[`qa/2026-06/`](qa/2026-06/) 2026-06-30；新探针 [`STATUS.md`](ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/STATUS.md)。

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
| ~~P0~~ | ~~T14~~ | ~~Encrypt 新探针 SIM 全链~~ — **✅ 已完成**（CPU+SIM c.bin max=0）。后续：晋级 stable / 性能优化 |
| 1 | T13b | vec-k4-v3（设备 `a_hat` + V3 预采样） |
| 2 | T11 | 2s1e exp → stable 晋级 |

---

## 5. smoke（拉代码后）

```bash
# Encrypt 新探针 CPU+SIM 全链（均应 PASS max=0）
cd ascendc-tests/fix-f203-alg14-encrypt-2launch-k4
ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
ENCRYPT_KERNEL_BUDGET_SEC=1000 ENCRYPT_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # ~13min

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
