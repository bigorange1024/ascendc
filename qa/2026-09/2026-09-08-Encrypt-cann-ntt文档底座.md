# 2026-09-08 · cann-ntt 核实 · Encrypt×Host 编排锁定 · 文档底座

## 用户当次口述（早间）

1. NPU 一时半会儿没法空闲 → 不强等上机。  
2. 曾述 AIV-only；后经核对 cann-ntt **实码为 MIX+BAT**，作者版本可能记混。  
3. **写码必须用 cannbot-skills**；图谱与知识库继续积累。

## 核实：作者称「单核…只用 AIV」

对照：`thirdparty/cann-ntt` 与作者包 `thirdparty/cann-ntt-author-merged_dsa/`。

| 说法 | 判定 |
|------|------|
| Kyber incomplete NTT **7 层** | 数学/golden **成立** |
| 设备核「小 N 纯 AIV」 | **不成立**；固定 MIX + Cube；`ntt_vec`=digit/Barrett |
| 入口 | 设备 = `mmad_custom` / OpenI `CubeNtt`；py 仅 Host golden |

作者包：Drive zip → `thirdparty/cann-ntt-author-new.zip` → `cann-ntt-author-merged_dsa/`（与 OpenI 并存）。

## 用户锁定（续讲 · 本线）

| 项 | 决定 |
|----|------|
| 路线 | Host 编排 + **迁入 cann-ntt 有用代码**（非 ACLNN 黑盒依赖） |
| 参数 | ML-KEM-**1024** |
| 目标 | **绝对不卡死**；正确性次要 |
| 目录 | `graph-tests/enc_cann_ntt/` |
| 主控 | 不写核；用 cannbot **设计**实验；派 subagent；刷 KB/DAG |
| 禁抄 | PKE/KEM 算子级；准全链可参考契约禁抄码 |

## 当次交付（文档底座 · 未编码）

| 产出 | 路径 |
|------|------|
| 能力清单 | `docs/notes/Encrypt-cann-ntt-capability-inventory.md` |
| 工作模式 | `docs/notes/Encrypt-cann-ntt-workmode.md` |
| 知识库 | `docs/notes/Encrypt-cann-ntt-kb.md` |
| DAG | `docs/rg-encrypt-cann-ntt.yaml`（`check_rg_dag` OK，14 nodes） |
| 试验场 INDEX | `graph-tests/enc_cann_ntt/INDEX.md` |

**下一动作**：确认后主控下发 **EN01**（迁入最小 KEM-256 NTT）；此前禁止写核。  
**Git**：无授权不 commit/push。

## EN01 已下发（同日续）

- 用户：NPU 长期占用 → **先 CPU+SIM**。  
- 任务书：`graph-tests/enc_cann_ntt/EN01-TASK.md`  
- 目录：`EN01-kem256-ntt-port/`（subagent 编码中）  
- 禁：`-r npu`。

## EN01 回收 · EN02 下发

- EN01：**PASS-NOHANG**（CPU+SIM；golden match；sync 红线 0；X30）。  
- EN02：`EN02-TASK.md` — Host NTT→INTT 双 launch 粘性；仍仅 CPU+SIM。

## EN02 回收 · EN03 下发

- EN02：**PASS-NOHANG**（双 launch；X31 M4 打包）。  
- EN03：`EN03-TASK.md` — Encrypt 形 Host 五段桩；仅 CPU+SIM。

## EN03 回收 · EN04 下发

- EN03：**PASS-NOHANG**（五段；X32 桩核 AIV_ONLY/SIM 占位）。  
- EN04：`EN04-TASK.md` — Matvec 换真积木；仅 CPU+SIM。

## EN04 回收 · EN05 下发

- EN04：**PASS-NOHANG**（真 Matvec；tick≈194125；X33）。  
- EN05：`EN05-TASK.md` — Prep 真采样；仅 CPU+SIM。

## EN05 回收 · EN06 下发

- EN05：**PASS-NOHANG**（PRF+CBD；Â 仍 Host；X34；tick≈246253）。  
- EN06：`EN06-TASK.md` — Pack Compress+ByteEncode；仅 CPU+SIM。

## EN06 回收 · EN07 下发

- EN06：**PASS-NOHANG**（Pack c=1568B；X35；tick≈296054）→ **五段积木形态齐**。  
- EN07：`EN07-TASK.md` — 段间真数据贯通；仅 CPU+SIM。

## EN07 回收 · EN08 下发

- EN07：**PASS-NOHANG**（贯通主路径；X36）。  
- EN08：`EN08-TASK.md` — 贯通链粘性多轮；仅 CPU+SIM。

## EN08 回收 · SIM 里程碑 · EN09 下发

- EN08：**PASS-NOHANG**（R=4；X37；SIM tick≈1.17M）。  
- **EN01–EN08 SIM 主路径里程碑**。  
- EN09：`EN09-TASK.md` — 设备 SampleNTT；仅 CPU+SIM。

## EN09 回收 · 暂停 SIM 加刀

- EN09：**PASS-NOHANG**（全 Â[4×4] SampleNTT→Matvec；X38；tick≈755040）。  
- **EN01–EN09 SIM 积木+贯通齐套** → 暂停同质 SIM 加刀。  
- 下一 P0：NPU 空闲后上机验绝对不卡死（须授权）。  
- Git：无授权不 commit/push。

## EN10 NPU 验收

- 用户：NPU 空闲；空闲&lt;4min 防关机；`cannlab-npu`=`100.68.205.47:2222`。  
- 接通：Tailscale userspace + SOCKS；修复 cannlab key PEM；`developer@`；910B3 / `ASCEND_DEVICE_ID=4`。  
- 跑 EN09 贯通链 `-r npu -v Ascend910B3` → **PASS-NOHANG**（kernel≈2.25s，全段 golden）。  
- 证据：`/opt/cursor/artifacts/EN10-npu.log`；KB S10/X39。

## EN11 多轮加压 · EN12 sticky 下发

- 用户：继续实验，勿让 NPU 空转。  
- EN11a/b 踩坑：裸跑缺 `LD_LIBRARY_PATH=out/lib`；`/usr/bin/time` 缺失误判 127；`pkill -f` 误杀 SSH（X40）。  
- EN11d：R=8 全 `BIN_RC=0` wall≈2–3s → **PASS-NOHANG**（进程级多轮）。  
- EN11e：R=200 加压保活中；校验用 `verify_result.py`。  
- EN12：`EN12-TASK.md` — 同进程 sticky SampleNTT（对齐 EN08 模式）；subagent 编码 CPU+SIM。  
- 证据：`/opt/cursor/artifacts/EN11d-npu-loop.log`。

## EN12 sticky NPU

- 初上机：`aclrtSetDevice(ASCEND_DEVICE_ID=4)` → aclError 107002 + segfault（**X41**）。  
- 修复：与 EN09 对齐 `deviceId=0`。  
- EN12b：`run.sh -r npu` R=32 **PASS-NOHANG** wall≈2.06s + golden；快路径 R=64 stress **PASS**。  
- EN12d：R=64×80 波加压保活中。  
- 证据：`/opt/cursor/artifacts/EN12b-npu.log`。
