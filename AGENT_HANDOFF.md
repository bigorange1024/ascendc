# Agent 交接 — Cloud / Local 交替用

> **最后刷新**：2026-09-08 20:10（Local 收工；下一棒 **Cloud：Decrypt/Decaps 整段重写**）  
> **读者**：任意 coding agent（含 Cursor Cloud）。先读本文件 → [`AGENTS.md`](AGENTS.md) → [`qa/INDEX.md`](qa/INDEX.md)。

---

## ★ 60 秒（真相）

1. **Encrypt / Encaps 已收口**（勿重开）：T22–T24 CPU+NPU；**liboqs** 交叉（T23 `c`/`K`）+ liboqs Decaps 往返（T24）+ 挂死压测 ×30。`Q-ULT` answered。  
2. **用户下一主线**：Decrypt / Decaps **整段从头重写**（同 Encaps 战役：砖块→设备 Decrypt→设备 Decaps→设备 Encaps↔Decaps 往返）。  
3. **现树 T25–T27 仅临时**：可参考 **STATUS/FEEDBACK/判决式教训**，**禁止当实现模板抄码**（与禁抄旧 `decaps/alg15/21` 同级）。  
4. **T27 已 NPU 绿（临时）**：根因不是密码学，是 **auto_gen 撞名**（见下「硬教训」）。  
5. Git 分支：`chore/thirdparty-add-cannbot-skills`（**无用户当次授权禁止另开分支 / 禁止擅自 commit·push**；本交接若用户说「推送」则可推）。

---

## P0（Cloud 接手立刻做）

| 序 | 做什么 | 验收 |
|----|--------|------|
| 1 | 在 `encrypt-rebuild-ops` **新开重写队列**（建议 `D-RW-*` / `RB-D*` 新目录前缀），**不要**在 `RB-T25/26/27` 上继续堆 | QUEUE 有重写总序；TASK 写清禁抄表 |
| 2 | 第一刀建议：**Decrypt 设备多 launch**（prep AIV → mid-sync → NTT/INTT MIX），对齐反卡死检查单 | CPU 绿即可交；NPU 有机再上 |
| 3 | 权威交叉：Decrypt↔liboqs；Decaps↔liboqs；最后 **设备 Encaps↔设备 Decaps** 关 `Q-RT-HANG` | 缺 liboqs → BLOCKED，禁 python 冒充 |
| 4 | 每日结束刷新 **本文件** + `QUEUE.md`；关键 X* 写入 KB/DAG | 下一棒可读 |

**非目标（本阶段）**：`examples/stable-*` 晋级；修 stable 旧 Encaps/Decrypt；在 T25–T27 上做大功能。

---

## 硬教训（重写必须遵守）

| ID | 教训 | 做法 |
|----|------|------|
| **撞名** | 同 binary 内两个源文件 basename 都叫 `prep_custom.cpp` → auto_gen 只留一个 → 另一 AIV 核从 `device_aiv.o` 消失 → NPU `LaunchAscendKernel ret 507000`、TRACE=0 | **每个 kernel `.cpp` basename 全局唯一**（如 `dec_prep_custom.cpp` / `enc_prep_custom.cpp`） |
| **X12** | `GlobalTensor::SetValue` 写业务 GM：CPU 假绿 / NPU 错字节 | H2D 直读 + UB + `DataCopy` 写出 |
| **反卡死** | 见 [`MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`](docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md) | 多 launch；flag **仅 1/3（+可选 4=GATE）**；禁 SoftSync / **5·7**；`BLOCK_DIM=1`；禁 fork 旧 `l18` |
| **预算** | 用户口径：~3 分钟出结果 | `KERNEL_COMPUTE_BUDGET_SEC` 默认 **180**；超时=有问题 |
| **NPU 主机** | 云主机重启换 IP/MagicDNS | `bash scripts/cannlab/which_npu.sh`；**勿写死** `cannlab-npu` |
| **liboqs** | 真机须 **aarch64** 本机编；不可 scp x86 `liboqs_kem_ref`（X11） | 远程 `build-liboqs` + ref |

---

## 已关闭台账（勿重复劳动）

| ID | 路径 / 结论 |
|----|-------------|
| T01–T21 | bricks / 拓扑探针（见 ops QUEUE 历史） |
| T22 | 设备 Encaps G |
| T23 | `RB-T23-encaps-liboqs-cross`：`c`/`K`≡liboqs Encaps |
| T24 | `RB-T24-encaps-decaps-roundtrip`：设备 Encaps→**liboqs** Decaps；×30 不挂 |
| T25 | `RB-T25-decrypt-device`：Decrypt 临时双绿（X12 后） |
| T26 | `RB-T26-decaps-device`：CPU 绿；撞名已改；NPU 可忽略（待重写） |
| T27 | `RB-T27-encaps-decaps-device-rt`：七 launch 往返 **NPU PASS**（撞名修复后） |

运营：[`graph-tests/encrypt-rebuild-ops/`](graph-tests/encrypt-rebuild-ops/) · KB：[`Encrypt-cannbot-rebuild-kb.md`](docs/notes/Encrypt-cannbot-rebuild-kb.md) · DAG：[`docs/rg-encrypt-cannbot-rebuild.yaml`](docs/rg-encrypt-cannbot-rebuild.yaml)

---

## 环境速查

```bash
# Cloud / 换机
bash scripts/clone-thirdparty.sh   # 默认 build liboqs

# NPU（用户开机后）
eval "$(bash scripts/cannlab/which_npu.sh --export)"
# 远程 worktree 惯例：/mnt/workspace/ascendc-encrypt-rebuild
# flock：/mnt/workspace/.npu.lock ；心跳：/mnt/workspace/.agent_heartbeat
# ASCEND_DEVICE_ID=0 ；soc 真机常 Ascend910B3

# 用例（默认即全量；预算 180s）
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r npu -v Ascend910B3   # 有卡；WSL 禁 npu
```

| 项 | 值 |
|----|-----|
| 分支 | `chore/thirdparty-add-cannbot-skills` |
| 工作模式 | [`Encrypt-cannbot-rebuild-work-mode.md`](docs/notes/Encrypt-cannbot-rebuild-work-mode.md) |
| 禁抄 | 旧 encrypt/encaps/decaps、`alg14/15/20/21`、`**/frozen/**`、**现 T25–T27 源码当模板** |
| 合法积木 | `graph-tests/bricks|toys`、活跃 Encaps 路径（T22/T23 **思路**）、`library/shared`、cannbot-skills |

---

## Local ↔ Cloud 交替约定

| 规则 | 说明 |
|------|------|
| **单一真相** | 开工读本文件；收工 **必须**改本文件「60 秒 + P0」 |
| **勿抢跑** | 用户未开 NPU → 只做 CPU/规格/队列；勿假绿 SIM/NPU |
| **Git** | 无「提交/推送」口头授权 → 只改工作区并汇报；有授权再 push |
| **冲突** | 两棒改同一文件：以较新 HANDOFF 时间戳 + `git pull --ff-only` 为准 |

---

## 粘贴给 Cloud 的短 Prompt

```text
先读 AGENT_HANDOFF.md、AGENTS.md、qa/INDEX.md、.cursor/rules/ascendc-development.mdc。
主线：Decrypt/Decaps 整段重写（新目录）；禁抄 T25–T27 与旧 decaps/alg15/21/frozen。
必守：kernel 源文件 basename 唯一；X12 DataCopy；反卡死 1/3(+4)/禁5·7/BLOCK_DIM=1；预算180s；liboqs 权威交叉。
Encaps 已收口勿重做。NPU 用 which_npu.sh。Git 分支 chore/thirdparty-add-cannbot-skills；无授权不另开分支、不擅自 push。
收工刷新 AGENT_HANDOFF.md + encrypt-rebuild-ops/QUEUE.md。
```
