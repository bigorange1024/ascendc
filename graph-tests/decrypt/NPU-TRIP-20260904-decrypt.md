# 实机测试清单 · Decrypt 卡死定位（给人看的版）

> 分支：`cursor/cann-ntt-operator-refactor-fe53`  
> 图谱：`docs/rg-decrypt-fused.yaml`  
> 目的：在 **真 NPU** 上看 Decrypt 会不会卡死、卡在哪一段。Cloud SIM 上 toy/stable **都没挂过**，所以这次是「上真机取证」，不是再验一遍 SIM。

---

## 0. 先搞清楚「卡」是什么

机器上有多张昇腾 NPU，每张有一个编号：`0、1、2、3…`。  
跑程序时用环境变量指定：

```bash
export ASCEND_DEVICE_ID=3   # 表示用 3 号那张卡
```

本仓库习惯（`scripts/npu_device_map.sh`）：

| 你跑的目录 | 默认用哪张卡 |
|------------|--------------|
| `ascendc-tests/...`（toy / 探针） | **3** |
| `examples/stable/...`（正式算子） | **1** |
| `examples/...` 其它实验 | **2** |

**为什么要分开**：以前某张卡上程序挂死后，**接着在同一张卡上再跑**很容易继续挂，看起来像算子坏了，其实是卡脏了。  
所以：toy 用 3 号；正式 Decrypt 用 1 号；**一旦挂死，先停，别在同一张卡上立刻再开下一个命令。**

WSL 不能跑 `-r npu`。必须在有真卡的 Linux 上。

---

## 1. 上机前：把代码拉到这台有卡的机器

```bash
cd /path/to/ascendc          # 你的仓库根
git fetch origin
git checkout cursor/cann-ntt-operator-refactor-fe53
git pull origin cursor/cann-ntt-operator-refactor-fe53

# 若这台机还没装过 thirdparty / liboqs（Decrypt golden 可能需要）：
bash scripts/clone-thirdparty.sh
```

确认头一次能看到新目录：

```bash
ls ascendc-tests/pass-toy-decrypt-fsm-fused-skel1/STATUS.md
ls examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/compute/g4_full/f203_decrypt_trace.hpp
```

---

## 2. 测试顺序（建议总共 30–45 分钟）

按 **A → B → C** 做。  
规则：**A 挂了就停，不要做 C。**

把终端输出整段保存（重定向或复制），每一步一个文件最好，例如：

```bash
mkdir -p /tmp/npu-trip-20260904
```

---

### 步骤 A（必做）· Decrypt 小玩具 · 用 3 号卡

测什么：轻量骨架（双 SoftSync + 双 GATE + NTT/INTT 握手），**不对密码学正确性**。  
问的是：真机上这个握手会不会卡死。

```bash
cd ascendc-tests/pass-toy-decrypt-fsm-fused-skel1

# 明确用 3 号卡；强制重编，避免旧二进制
export ASCEND_DEVICE_ID=3
FORCE_REBUILD=1 TOY_LAUNCH_REPEAT=8 bash run.sh -r npu -v Ascend910B4 \
  2>&1 | tee /tmp/npu-trip-20260904/A-decrypt-toy.log
```

**怎样算过：**

- 命令正常结束（exit 0）
- 日志里有类似 `synchronize done` / `SUCCESS`
- 有 TRACE 打印（槽位数字）
- **没有**一直卡在同步、也没有超时退出码 **124**

**怎样算挂：**

- 很久不动，或 exit **124**
- 记下：卡了多久、第几轮（若有 `iter=`）、日志最后几行、`ASCEND_DEVICE_ID`

→ **停。不要做 B/C。** 把 `A-decrypt-toy.log` 带回来。

---

### 步骤 B（建议）· Encrypt 小玩具 · 仍用 3 号卡

测什么：Encrypt 那条 toy 骨架，对照用。  
**前提：A 已经正常结束。**

```bash
cd ascendc-tests/pass-toy-encrypt-fsm-l18-skel1

export ASCEND_DEVICE_ID=3
FORCE_REBUILD=1 TOY_LAUNCH_REPEAT=8 bash run.sh -r npu -v Ascend910B4 \
  2>&1 | tee /tmp/npu-trip-20260904/B-encrypt-toy.log
```

**怎样算过：** exit 0；有 `[toy-l18-trace]` 一类打印；不超时。  
挂了同样停，把日志带回。

---

### 步骤 C（核心）· 正式 Decrypt + 定位 TRACE · 用 1 号卡

测什么：你指出的那条 **生产路径**：`input/` 里只有 `dk_pke + c + lut_*`，跑 `stable-fips203-mlkem-pke-decrypt-k4`。  
打开 `F203_DECRYPT_TRACE=1`：若卡在同步，终端会每隔约 0.5s 打已经走到哪一段。

**前提：A 已通过。**  
换到 **1 号卡**（正式算子约定卡；也避免和 toy 刚用过的 3 号搅在一起）。

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4

# 换卡：正式算子用 1
export ASCEND_DEVICE_ID=1

# TRACE 定位 + 定点种子（方便复现）+ 强制重编
FORCE_REBUILD=1 F203_DECRYPT_TRACE=1 SEED_D=20260619 \
  bash run.sh -r npu -v Ascend910B4 \
  2>&1 | tee /tmp/npu-trip-20260904/C-stable-decrypt-trace.log
```

日志开头应出现类似：

```text
[main_decrypt] F203_DECRYPT_TRACE=1：轮询 fused-trace + softSync
[gen_data] prod input = dk_pke + c + lut_* only; ...
```

**结果怎么读：**

| 你看到什么 | 说明 |
|------------|------|
| `[verify] PASS` / `[SUCCESS]` | 这个 seed 在真机上 **没挂且对拍过** |
| 一直卡住，或 exit **124** | 当挂死；看下面「卡在哪」 |
| 反复打印 `[decrypt-trace] stages set=…` | 正常：还在跑或已卡住时在汇报进度 |

**卡在哪（看最后一条 `[decrypt-trace]`）：**

格式大概是：

```text
[decrypt-trace] stages set=5/17 : 0 1 2 3 4 | softSync=[1,0]
```

| 已出现的槽号大概停在 | 含义 |
|----------------------|------|
| 0–2 | prep / SoftSync 第 0 槽 / prep 后的 GATE |
| 3–5 | NTT |
| 6–8 | su_dot / SoftSync 第 1 槽 / 第二段 GATE |
| 9–12 | INTT / 收尾写 m |
| 13–16 | AIC 侧标记（可能假空，**别只信这些**；优先看 0–12 和 softSync） |

`softSync=[s0,s1]`：若一直是 `[0,0]` 而 AIV1 该等 slot，可能卡在 SoftSync 忙等；若已是 `[1,…]` 说明那一槽已置位。

**超时：** 默认预算约 600 秒；124 当挂死，**不要在同一张卡上立刻再跑一遍 C**。

---

### 步骤 D（可选）· Encaps TRACE · 仍建议 1 号卡

只在时间够、且 **C 没把卡弄挂**（或你已经 reset / 换干净卡）时再做：

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
export ASCEND_DEVICE_ID=1
F203_L18_TRACE=1 bash run.sh -r npu -v Ascend910B4 \
  2>&1 | tee /tmp/npu-trip-20260904/D-encaps-trace.log
```

这是 Encrypt 线对照，日志和 Decrypt **分开存**。

---

## 3. 请带回给我的东西

打包或直接贴：

```bash
ls -la /tmp/npu-trip-20260904/
# 至少：A-decrypt-toy.log ；若做了 C：C-stable-decrypt-trace.log
```

务必包含：

1. 每步完整终端输出（含 TRACE、SUCCESS/FAIL、exit）  
2. 你设的 `ASCEND_DEVICE_ID`  
3. 若挂死：大概挂了多久、是否 124、**最后一条** `[decrypt-trace]`  
4. 不必传整包 OPPROF / dump

---

## 4. 不要做的事

- A 已经挂了还硬跑 C  
- 挂死后在同一张卡上连环重跑 Decrypt/Encaps  
- 以为 toy 绿了 = 正式 Decrypt 消粘完成  
- 改生产 FSM（先把日志给我，再决定下一刀）
