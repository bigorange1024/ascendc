# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-30（晚 · Encrypt G5 + Decrypt G4 + **PKE round-trip CPU+SIM**）

---

## ★ SIM 测试通过声明（2026-06-30）

### Encrypt

**结论：`fix-f203-alg14-pke-encrypt-correctness-k4` SIM 路径完整测试通过**（507000 病根治愈，单 ACL session + `at_r5` + 全 device G4）。

| 项 | 内容 |
|----|------|
| 命令 | `bash run.sh -r sim -v Ascend910B4` |
| 工作目录 | `ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/` |
| 关键日志 | `[verify] PASS max=0 (1568 bytes)` |
| `Total tick` | **922441** |

### Decrypt（新增）

**结论：`fix-f203-alg15-pke-decrypt-correctness-k4` CPU+SIM G1–G4 max=0**（2 host launch；Launch-2 内 ntt+intt 两 kernel）。

| 项 | 内容 |
|----|------|
| 命令 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` |
| 工作目录 | `ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/` |
| 关键日志 | G1–G4 max=0（`m.bin` 32B） |
| `Total tick` | **~427k** |
| 定稿 note | [`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md) |

详 Encrypt [`STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) · Decrypt [`STATUS.md`](ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/STATUS.md)

### PKE round-trip（device 闭环，新增）

**结论**：KeyGen 密钥 → device Encrypt `c.bin` → device Decrypt `m.bin`；**CPU+SIM 对拍 max=0**（32B）。

| 项 | 内容 |
|----|------|
| 脚本 | [`scripts/roundtrip_pke_encrypt_decrypt.sh`](scripts/roundtrip_pke_encrypt_decrypt.sh) |
| 密钥 | `pass-fix-f203-alg13-device-keygen-k4/output/`（`ROUNDTRIP_BOOTSTRAP_KEYGEN=1` 可缺省 bootstrap） |
| SEED_D | 20260619 |
| SIM 耗时 | Encrypt ~633s + Decrypt ~253s（wall；tick Decrypt ~427k） |

```bash
bash scripts/roundtrip_pke_encrypt_decrypt.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/roundtrip_pke_encrypt_decrypt.sh -r sim -v Ascend910B4
```

> 单探针 `run.sh` 仍用 host golden；round-trip **补充**跨算子 device I/O 闭环，不替代各探针 oracle 验收。

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 §1（Encrypt + **Decrypt**） |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

---

## 1. 当前真相（2026-06-30 晚）

### Encrypt Alg.14（G5 双模式 PASS）

| 路径 | [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) |
|------|------|
| **验收** | 默认 `bash run.sh` → CPU+SIM **c.bin max=0** 1568B |
| **G3** | **`at_r5` 合并核**（kP=5）；旧 G3 四核 → `compute/frozen/` |
| **已关闭** | 家里分叉 [`frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/`](ascendc-tests/frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/) |

### Decrypt Alg.15（**G4 2-launch PASS — 新交付**）

| 路径 | [`ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/`](ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/) |
|------|------|
| **验收** | CPU+SIM **m.bin max=0** 32B（G1–G4） |
| **Launch** | **2×** `aclrtLaunchKernel`：`prep` → `chain_ntt` + sync + `chain_intt` |
| **SEED_D** | 20260619（与 Encrypt round-trip 共用 golden） |

**踩坑（勿重试）**：

| 尝试 | 结果 |
|------|------|
| prep+NTT 同 kernel | SIM `û` 错（coeff 0） |
| NTT+INTT 同 kernel | SIM `m` 错 |
| CrossCore flag 4/0/10 段间握手 | SIM **死锁** 10min+ |
| 6 launch 多 sync | 正确但 tick ~405k；2-launch 为当前最小切分 |

**定稿**：[`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md) · 纪要 §15 [`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](qa/2026-06/2026-06-30-funckey-507000本地独立验证.md)

```bash
cd ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

### SIM 单 session 两大病根（Encrypt 已吸收）

| 病根 | 解法 |
|------|------|
| R1：AIV `func_key ≥ 5` → 507000 | SIM AIV-only ≤5；`at_r5` 合并核；decode/pack MIX 占位 |
| R2：D2H 前缺 `aclrtSynchronizeStream` | matM 打包前显式 sync |

Decrypt 同样：**每 kernel 后 sync**；prep 用 MIX 占位释 SIM device 路径。

### KeyGen（k=4，生产路径）

| 角色 | 路径 | 状态 |
|------|------|------|
| **stable 交付** | [`examples/stable/stable-mlkem-f203-pke-keygen-k4/`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | CPU/SIM/KAT ✅；SIM **542393** tick |
| **探针** | [`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | 同上（prep **双 AIV 并行 Â**） |

**定稿**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)

### ML-KEM NTT / 2s1e 向量基线

| 路径 | 说明 |
|------|------|
| [`ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/`](ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | Alg.13 行 16–20 全链；SIM **77958** tick |

### 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## 2. GitHub 同步

**推荐**：`git pull origin main`

### `thirdparty/` — 本地依赖（**不进 GitHub**）

`.gitignore` 排除整个 `/thirdparty/`；**禁止** `git add thirdparty/`、禁止 `git clean -fdx` 后不重装就报「探针 PASS」。

| 目录 | 来源 | 用途 |
|------|------|------|
| `ntt_study/` | 用户本机 tree / 与 `~/ntt_study` 同步 | golden 对照、`mlkem_ref.py`、文档 |
| `merged_kyber/` | 用户本机 tree | frozen 探针脚本、MIX FSM 参考 |
| `tiny_sha3/` | `git clone https://github.com/mjosaarinen/tiny_sha3` | Host SHA3/SHAKE golden（[`mjosaarinen/tiny_sha3`](https://github.com/mjosaarinen/tiny_sha3)） |
| `liboqs/` | tag **0.15.0** clone + build | KAT / `keygen_golden.py` |

**新机器 / `git clean -fdx` 后**（拉代码**不会**带回 thirdparty）：

```bash
# tiny_sha3（公开 upstream）
git clone --depth 1 https://github.com/mjosaarinen/tiny_sha3.git thirdparty/tiny_sha3
# ntt_study、merged_kyber、liboqs：从办公室备份或 ~/ntt_study 等拷贝/软链
```

**Agent 禁令**：勿删 `thirdparty/` 子目录；勿把 thirdparty 内容 commit 进 ascendc 仓。

---

## 3. 建议下一步（见 [`qa/TODO.md`](qa/TODO.md)）

| 优先级 | ID | 事项 |
|--------|-----|------|
| P1 | **T13b** | vec-k4-v3（V3 预采样 + 设备 `a_hat`） |
| P2 | **T11** | 2s1e exp → stable 晋级 |
| P3 | **T14a** | Encrypt G5 → stable 晋级 |
| P4 | **T15a** | Decrypt G4 → stable 晋级 |

---

## 4. smoke（拉代码后）

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

cd ../fix-f203-alg15-pke-decrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# 跨探针 device 闭环（KeyGen 密钥 → Encrypt c → Decrypt m）
bash ../../scripts/roundtrip_pke_encrypt_decrypt.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash ../../scripts/roundtrip_pke_encrypt_decrypt.sh -r sim -v Ascend910B4

cd ../../examples/stable/stable-mlkem-f203-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
```

---

## 5. 索引

| 主题 | 链接 |
|------|------|
| Encrypt STATUS | [`fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) |
| Decrypt STATUS | [`fix-f203-alg15-pke-decrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/STATUS.md) |
| Decrypt 2-launch note | [`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md) |
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
