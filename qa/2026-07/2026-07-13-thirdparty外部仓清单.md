# 2026-07-13 — thirdparty 外部仓清单与一键 clone

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

## 备注

- `SHA3hp` / `cann-ntt`：第三方 AscendC Keccak / NTT，仅调研对照，未论证前不替换活跃实现。
- Skill：`ascendc-engineering-notes` 已改 `ntt_onnx` + `pass-merged-kyber-mix-ntt256`（用户确认后更新）。
