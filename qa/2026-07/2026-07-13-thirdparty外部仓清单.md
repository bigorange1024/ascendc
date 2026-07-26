# 2026-07-13 — thirdparty · KEM KeyGen 规格重置 · T21

## 决策

`thirdparty/` 仍 **整体不进 Git**（`.gitignore`：`/thirdparty/`）。下列仓为外部 clone，换机须能复现：

| 目录 | URL | 钉住 |
|------|-----|------|
| `tiny_sha3/` | https://github.com/mjosaarinen/tiny_sha3.git | 默认分支浅克隆 |
| `liboqs/` | https://github.com/open-quantum-safe/liboqs.git | **tag 0.15.0** |
| `ascend-samples/` | https://gitee.com/ascend/samples.git | 默认分支浅克隆 |
| `SHA3hp/` | https://openi.pcl.ac.cn/wtUSTB/SHA3hp.git | 默认分支浅克隆 |
| `cann-ntt/` | https://openi.pcl.ac.cn/serial2007/cann-ntt.git | 默认分支浅克隆 |
| `ntt_onnx/` | https://github.com/bigorange1024/ntt_onnx.git | 默认分支浅克隆（原 `ntt_study`） |

**入口**：`bash scripts/clone-thirdparty.sh`（已存在则跳过；`FORCE=1` 重拉）。

文档权威：[docs/engineering/thirdparty-本地依赖.md](../../docs/engineering/thirdparty-本地依赖.md)；README / 环境复现指南已挂链。

## §2 merged_kyber 迁入 ascendc-tests（同日）

| 项 | 内容 |
|----|------|
| 决策 | 作者已授权；**不再**保留 `thirdparty/merged_kyber` |
| 落点 | [`ascendc-tests/pass-merged-kyber-mix-ntt256/`](../../ascendc-tests/pass-merged-kyber-mix-ntt256/)（`STATUS.md` / `ORIGIN.md`）；CPU+SIM PASS |
| 引用 | 活跃探针 `MERGED_KYBER_ROOT` 改本目录自包含；`exp-sepolyvec8` 的 `ntt_sim_kyber` 指向新用例 `scripts/` |
| 说明 | 本用例 = 上游示例本体；frozen `frozen-*-merged-kyber-*` 仍为已关闭研究 fork，勿混淆 |

**Skill 路径**：`.cursor/skills/ascendc-engineering-notes/` 已改指向 `thirdparty/ntt_onnx` 与 `pass-merged-kyber-mix-ntt256`（用户确认后更新）。

## §3 ntt_study → ntt_onnx（同日）

| 项 | 内容 |
|----|------|
| 上游 | https://github.com/bigorange1024/ntt_onnx.git |
| 本机路径 | `thirdparty/ntt_onnx/`（**已删除** `thirdparty/ntt_study/`） |
| 仓内引用 | `thirdparty/ntt_study` → `thirdparty/ntt_onnx`（含 vendored 子树改名） |
| 脚本 | `clone-thirdparty.sh` 已纳入；私有仓 HTTPS 失败时回退 `gh repo clone` |

## §4 Alg.19 KEM KeyGen incubating 规格（同日）

| 项 | 内容 |
|----|------|
| 触发 | `$写规格$`（ascendc-impl-spec）；基线 `pass-fix-f203-alg19-kem-keygen-device-k4` |
| 落点 | [`examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4/`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4/) |
| 产出 | [`…-实现方案-customspec.tex/.pdf`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4/exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf) |
| 锁定 | 2 launch；ek\_kem 1568 / dk\_kem 3168；incubating **vendored 自包含**（异于探针 `STABLE_ROOT`）；`F203_KEM_KEYGEN_TAIL=1` |
| 状态 | 规格已确认 → §5 写码 |

## §5 Alg.19 KEM KeyGen 【预研】写码（同日）

| 项 | 内容 |
|----|------|
| 实现 | vendored PKE + `kem/` 尾；CMake/run.sh 自包含 |
| 验收 | CPU **PASS**；SIM **PASS** Total tick **713241**；ek/dk max=0 |
| TODO | 新增 **T21**：分析 `thirdparty/SHA3hp` 替换 SHA3-256/512；**T19f** 记 incubating PASS |
| 未做 | `#交付#` 晋级 stable |

## §6 T21 SHA3hp 对照讨论（同日）

| 结论 | 说明 |
|------|------|
| SHA3hp 内容 | 仅 **KeccakF1600 + SHAKE128/256**；**无** SHA3-256/512 算子 |
| 与先前 SHAKE128 | **同系**：ops-math `shake128_general`；本仓 `library/shared/shake_xof_kernel` 已派生（UB/`ProcessInline`） |
| keccak 头 | `SHA3hp/.../keccak_f1600.h` 与 `library/shared/keccak_f1600_kernel/keccak_f1600.h` **实质同文**（仅 shared 多文件头注释） |
| 改 SHA3-256/512 | 算法上改 rate=`200-2·mdlen`、域分隔 **0x06**（非 SHAKE 的 0x1F）；**勿**把 SHAKE256 当 SHA3-256 |
| 性能预期 | SHA3hp permute 仍是 **标量 uint64 状态机**，非矢量 Keccak；当前 `F203SeDeviceKeccak::Sha3OneShot` **已调用同一 `PermuteChain`** |
| 晋级 stable | 用户前提：SHA3 问题先拍板；T21 仍打开 |

## §7 `#验收#` KEM KeyGen → stable（同日，**已撤回**）

| 项 | 内容 |
|----|------|
| 操作 | 曾自 exp 复制到 [`stable-fips203-mlkem-kem-keygen-k4/`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/) |
| 问题 | 用户指出：incubating CPU 偶发对拍未根治即晋级不当 |
| 处置 | **stable 标「晋级撤回 / 非权威」**；权威仅 incubating；T19f **重开** |

## §8 incubating 同步修复与复验（同日）

| 项 | 内容 |
|----|------|
| 根因（初判） | `KYBER_PIPE_ALL` 空操作 → Fuse/Tail 本核「半新旧」 |
| 初修 | 真实 `PipeBarrier`；`run.sh` 源码新鲜度强制 rebuild |
| 仍失败 | CPU 压测仍偶发 `dk` FAIL |

## §9 根因锁定与曾重晋级（同日，**已被 §10 取代**）

| 项 | 内容 |
|----|------|
| 精确定位 | FAIL 时 **仅** `dk_pke[1152:)` 错（≈384B / AIV1 末 poly）；`ek`/`H`/`z`/`ek_in_dk` 全对 |
| 根因 | AIV0 在 AIV1 写完 `sk_out` 前执行 Fuse/Tail；本核 `PipeBarrier` 无法汇合双 AIV；残留 GM 曾掩盖偶发 |
| 曾用修复 | 设备 `SyncAll<isAIVOnly=true>()` 后 AIV0 做尾；CPU 由 `subBlockID==1` 做尾；`KYBER_PIPE_ALL` 恒真实 |
| API 查阅 | 索引追加 SyncAll §2.3.7.2.3 p.1086 |
| 曾验收 | CPU **40/40** + SIM **709778**（但用户否决「stable/incubating 双轨」维护方式） |

## §10 用户裁决：删实现、只留规格（同日）

| 项 | 内容 |
|----|------|
| 意见 | incubating 与 stable 双份实现易漂移；出错先改谁不清晰；过早晋级不当 |
| 操作 | **整树删除** `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/`；**删除** incubating 内全部源码/脚本/build，**仅留** customspec `.tex`/`.pdf` |
| 规格 | 已写入 **§踩坑与强制同步（landmines）**：SyncAll、CPU AIV1 做尾、禁空 `KYBER_PIPE_ALL`、禁残留侥幸验收、禁未绿晋级 |
| 交接 | 刷新根 [`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md)；T19f →「按规格【预研】从零重写」 |
| 回家任务 | 家里 Agent 读 PDF + registry，**【预研】** 在 incubating 重写；**勿**先建 stable |

## 备注

- `SHA3hp` / `cann-ntt`：第三方 AscendC Keccak / NTT，仅调研对照，未论证前不替换活跃实现。
- Skill：`ascendc-engineering-notes` 已改 `ntt_onnx` + `pass-merged-kyber-mix-ntt256`（用户确认后更新）。
