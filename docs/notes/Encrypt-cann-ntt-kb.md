# Encrypt × cann-ntt · 专属知识库

> **核心问题**：ML-KEM-1024 Encrypt 在 **Host 多段 + 迁入 cann-ntt NTT** 下 **绝对不卡死**（正确性次要）。  
> **维护**：主控；每刀短更新；失败优先；无杂讯。  
> **读者**：主控 + **subagent（只读）**。  
> **配套**：[能力清单](Encrypt-cann-ntt-capability-inventory.md) · [工作模式](Encrypt-cann-ntt-workmode.md) · [DAG](../rg-encrypt-cann-ntt.yaml) · 试验场 [`graph-tests/enc_cann_ntt/`](../../graph-tests/enc_cann_ntt/)

---

## 0. 锁定结论（2026-09-08）

| ID | 事实 |
|----|------|
| L1 | 路线 = Host 编排 + **把 cann-ntt 有用代码迁入工程** 做 NTT/INTT（非外部 ACLNN 黑盒依赖）。 |
| L2 | 参数 = **1024 / k=4**。 |
| L3 | 主目标 = **不卡死**；正确性非主门禁。 |
| L4 | 落点 = `graph-tests/enc_cann_ntt/`。 |
| L5 | 主控不写核；subagent 编码；实验设计用 `thirdparty/cannbot-skills`。 |
| L6 | 禁抄 PKE/KEM 算子级实现；准全链探针可参考契约、禁抄码（NTT 已换）。 |

---

## 1. 架构不变量（算法 / 工程）

| ID | 不变量 | 含义 |
|----|--------|------|
| A1 | NTT/INTT **独立 launch** | 握手关在迁入的短 MIX NTT 内；Encrypt 业务段不与之融成单胖核 |
| A2 | Prep / Matvec / Pack 默认 **AIV_ONLY** | 避免 AIC 空等 AIV 大体量（旧卡死画像） |
| A3 | 禁止自研 Encrypt GATE **4/8** 与 Cube 二段复用同核 | 旧 hang 线已证明该拓扑高风险 |
| A4 | NTT I/O 以 **迁入 cann-ntt 布局** 为准 | 旧 Tag5T/UB 融合布局不默认兼容；Matvec 须重接 |
| A5 | `n=256,q=3329`；BAT **2-digit**（KEM 路径） | 与 OpenI cann-ntt ML-KEM 验收档一致 |
| A6 | 一刀一目录；每刀 cannbot **sync_audit**（有同步则必跑） | 红线禁否决 |

---

## 2. 从旧 hang 线继承的禁令（只读继承 · 勿复验当新发现）

来源：`Encrypt-hang-rewrite-kb.md`（摘要）：

| ID | 禁令 |
|----|------|
| B1 | INTT 换 CrossCore flag **5/7** → 永禁 |
| B2 | AIC 仍在 CrossCore Wait 时 `SyncAll` → 永禁 |
| B3 | 自造 AIC↔AIV SoftSync / 乱 recreate stream → 永禁 |
| B4 | 照抄现有 Encrypt 核 → 永禁 |
| B5 | 仅加 Host launch / 假 UB 循环 / 同质 SIM 体量加压当「已解卡死」→ 不充分 |
| B6 | CPU 绿不作卡死因果证据 |
| B7 | sync_audit 红线不可主观否决（ER02/X25） |

---

## 3. cann-ntt 迁入要点（工程）

| ID | 知识点 |
|----|--------|
| C1 | OpenI 树：`thirdparty/cann-ntt/`；内核 MIX `CubeNtt`；KEM：N=256、q=3329、2-digit BAT。 |
| C2 | 作者 `merged_dsa` 包：同族矩阵化 NTT；`ntt_vec.hpp`=digit-split/Barrett **不是** butterfly NTT。 |
| C3 | 本线不依赖「小 N 走纯 AIV」口述；以仓库实码为准。 |
| C4 | 迁入范围（预期）：设备 split/mmad/merge、tiling、host launch 壳、KEM 矩阵生成脚本；**不迁** ACLNN 注册全栈（除非单刀子项证明需要）。 |
| C5 | 迁入后第一刀应 **单独** 跑通 NTT（及可选 INTT），再挂 Encrypt 编排。 |

---

## 4. 算法能力锚点（详见能力清单）

Encrypt 需要且仓内已有（禁算子级抄码）：SHAKE/Keccak、SampleNTT、CBD η₂、MultiplyNTTs/内积、ByteEncode/Decode、Compress/Decompress、mod_q。  
**缺口**：工程内可 launch 的 cann-ntt NTT；Host 多段 Encrypt 壳；Matvec↔新 NTT 布局对齐。

---

## 5. 实验台账（本线）

| 刀 | 目录 | 结果 | 沉淀 |
|----|------|------|------|
| EN01 | `EN01-kem256-ntt-port/` | **PASS-NOHANG** | X30；N0 |
| EN02 | `EN02-ntt-intt-2launch/` | **PASS-NOHANG** | X31；INTT+双 launch |
| EN10 | NPU 真机跑 EN09 链 | **PASS-NOHANG**；910B3 `davinci4`；全段 golden match | X39；`Q-ULT` 本路径有条件证据 |
| EN11 | NPU 连续多轮（进程级）EN09 | **PASS-NOHANG**；EN11d R=8；EN11e R=200 全过（soft=0 hang=0） | S11；X40 |
| EN12 | NPU 同进程 sticky SampleNTT 链 | **PASS-NOHANG**；R=32 run.sh + R=64 stress；golden | S12；X41 |

### 失败（优先）

| ID | 结论 | 含义 |
|----|------|------|
| （空） | 本线尚无导致挂死的失败项 | — |

### 成功 / 教训

| ID | 结论 | 含义 |
|----|------|------|
| S0 | 文档底座齐 | 可开 EN01 |
| S1 | EN01 迁入 KEM-256 正向 NTT：CPU+SIM 不挂；tick≈11356；红线 0 | **N0 积木可用**；可进 EN02（INTT / 双 launch） |
| S2 | EN02 Host NTT→INTT 双 launch 不挂；tick≈20685；红线 0 | **INTT + 粘性双 launch** 可用；可进 EN03 Host 多段桩 |
| S3 | EN03 Encrypt 形五段 Host 编排不挂；tick≈29824；红线 0 | **G2 壳可用**；下刀换真 Matvec/Prep 积木 |
| S4 | EN04 真 Matvec（P-inner-1+paired basemul）夹在 NTT/INTT 间不挂；tick≈194125 | **M1/M2 可接入 Host 壳**；体量↑但未挂 |
| S5 | EN05 设备 PRF+CBD η2×4 作 Prep 不挂；tick≈246253；Â/SampleNTT 仍 Host | **S2 可接入**；S1 设备端仍缺口 |
| S6 | EN06 全密文 Pack（Compress₁₁/₅+ByteEncode→1568B）不挂；tick≈296054 | **五段积木形态齐**（除设备 SampleNTT）；下刀做**段间真贯通** |
| S7 | EN07 段间贯通 Prep.y→NTT→Matvec.ŷ→INTT→Pack.u 不挂；tick≈296064 | **Host 编排主路径可贯通**；Â/γ/v 仍 Host |
| S8 | EN08 贯通链同 session R=4 粘性不挂；SIM≈180s / tick≈1.17M | **SIM 上粘性多轮未复现挂**；NPU 仍待验 |
| S9 | EN09 设备 SampleNTT 全 Â[4×4] 直喂 Matvec 不挂；tick≈755040 | **S1 闭合**；SIM 侧 Encrypt 形积木+贯通基本齐套 |
| S10 | EN10 NPU(910B3/dev4) 跑 EN09 贯通链 **不卡死** 且全段 golden match | **真机不挂证据**（本 Host 编排路径）；非旧 l18 胖 MIX |
| S11 | EN11 NPU 进程级连续多轮 EN09（EN11d R=8；EN11e R=200 soft=0 hang=0）**不挂** | **多轮重启 session 亦不挂** |
| S12 | EN12 NPU sticky SampleNTT 链 R=32（run.sh）+ R=64 stress **不挂** 且 round-1 golden | **真机同 session 粘性多轮不挂**；wall(R=32)≈2.06s |
| X38 | 独立 AIV + shared SHAKE 串行 16 poly 控 UB | 勿抄 Encrypt/alg7 整核 |
| X39 | 真机：MagicDNS `cannlab-npu` / `100.68.205.47:2222`；userspace TS 须 SOCKS；key 需 PEM 头；`ASCEND_DEVICE_ID=4`；SOC=`Ascend910B3`；空闲&lt;4min | 发现勿写死旧 IP；保活 ServerAlive+作业心跳 |
| X40 | 快路径须 `LD_LIBRARY_PATH=out/lib`；校验脚本名 `verify_result.py`；禁 `pkill -f` 匹配自身 SSH 命令行；勿依赖 `/usr/bin/time` | 远程循环脚本先落地文件再 nohup |
| X41 | Host `aclrtSetDevice` 必须逻辑 **0**（EN01–EN11 惯例）；`ASCEND_DEVICE_ID=4` 只给 run.sh 选卡，不可 SetDevice(4)→107002+segfault | sticky 勿误读 env 改 deviceId |

---

## 6. TRACE（上机时）

100–199 Host / 200–299 AIV0 / 300–399 AIV1 / 400–499 AIC / 900+ 异常。

---

## 7. 下一刀

- EN11/EN12 NPU：**PASS-NOHANG**（进程多轮 + sticky）。  
- EN12c：R=128×5 波 sticky 加压保活中。  
- 可选：对照旧 Encrypt l18（只读禁抄）。空闲 **&lt;4 min**。
