# ML-KEM PKE Encrypt（k=4）需求文档（cannbot CP1 草稿）

## 修订记录

| 版本 | 修订内容 | 修订时间 | 修订人 |
|-----|---------|---------|--------|
| v0.1 | 按 cannbot `ascendc-docs-gen` 需求分析模板起草；绑定本仓卡死重写约束 | 2026-09-07 | cloud-agent |

> **状态**：草稿，待用户 CP1 拍板后升 v1.0。  
> **模板来源**：`thirdparty/cannbot-skills/ops/ascendc-docs-gen/references/requirement-analysis-template.md`  
> **架构依据**：`thirdparty/cannbot-skills/ops/npu-arch`（910B 系 → **DAV_2201 / MemBase / MIX SIMD**）

---

## 1. 需求背景

- **需求来源**：本仓 ML-KEM-1024 Encrypt 在 NPU 上出现 `aclrtSynchronizeStream` 粘性卡死；用户要求用 **cannbot Ascend C 直调** 技能链 **从头重做** Encrypt，达到无卡死且正确。
- **基线对齐**：
  - [x] 标准：NIST FIPS 203 · K-PKE.Encrypt（Alg.14）
  - [x] 参数组：ML-KEM-1024（\(k=4,\eta_1=2,\eta_2=2,d_u=11,d_v=5\)）
  - [ ] 框架 API（PyTorch 等）：本阶段 **不需要**
  - [x] 权威交叉（后置 CP3）：liboqs / KAT（有 thirdparty 时）
- **非目标（首期）**：ACLNN 注册上架、图模式、性能打满、抄现有 `stable-…-encrypt-k4` 核实现。

### 模型/流水结构（融合）

Encrypt 是多阶段融合直调算子链（非单点 elementwise）：

1. 采样/展开：由 \(ek\)、\(m\)、\(coins\) 得 \(\hat{A}\)、噪声与消息嵌入等（可 Host 折 μ 或设备采样，方案阶段拍板）
2. NTT / 矩阵-向量 / 内积 / INTT
3. Compress + ByteEncode → 密文 \(c=c_1\|c_2\)

卡死历史集中在 **MIX 计算核**（类 `l18_l19`：NTT + GATE 大体量 + INTT）的 CrossCore 编排，而非积木算术本身（见 `docs/notes/Encrypt-hang-rewrite-kb.md`）。

---

## 2. 运行环境

| 项 | 目标 |
|----|------|
| 主验收芯片 | **Ascend 910B3**（用户云主机；机时稀缺） |
| 兼容对照 | Ascend 910B4（原办公室/既有 SIM 矩阵） |
| NpuArch | **DAV_2201**（cannbot `npu-arch`：910B1–B4） |
| 编程形态 | Ascend C **Kernel 直调**（非注册调用）；SIMD **MemBase**；**MIX**（AIC+AIV） |
| 迭代验证 | Cloud/WSL：**CPU 孪生 + CAModel SIM**；最终：**NPU** |
| 编译宏 | 与本仓既有 `Ascend910B4`/`SOC_VERSION` 惯例对齐；910B3 实机以环境探测为准 |

---

## 3. 调用方式

| 调用方式 | 是否支持 |
|---------|---------|
| Kernel 直调（`ACLRT_LAUNCH_KERNEL` / 本仓 KernelLaunch） | **是（唯一主路径）** |
| ACLNN 调用 | 否（首期） |
| torch_npu / torch.compile / GE | 否 |

---

## 4. 算子规格

### 4.1 基本信息

- **算子名称（工作名）**：`mlkem_pke_encrypt_k4`（目录名待 CP1 拍板）
- **数学**：FIPS 203 Alg.14（K-PKE.Encrypt），参数 ML-KEM-1024
- **仓规划**：新树实现；旧 `examples/stable/.../stable-fips203-mlkem-pke-encrypt-k4` **只读参考 I/O**，禁止抄核

### 4.2 输入输出规格（黑盒；尺寸对齐本仓 1024 交付约定）

#### 输入

| 符号 | 含义 | 字节（1024） |
|------|------|----------------|
| `ek_PKE` | K-PKE 公钥 | 1568 |
| `m` | 消息 | 32 |
| `coins` | 随机币 | 32 |
| （实现相关）NTT/INTT LUT 等 | 只读表 | 方案阶段锁定 |

#### 输出

| 符号 | 含义 | 字节 |
|------|------|------|
| `c` | 密文 \(c_1\|c_2\) | **1568** |

中间量（\(u,v,\hat{y}\) 等）可仅设备内，不强制落盘。

### 4.3 数据类型

- 字节流 / 域元素打包：以 FIPS 203 编解码为准（非 fp16 训练算子精度口径）
- 设备内部：int16/int32 等按既有 ML-KEM 探针惯例（方案阶段写死）

### 4.4 正确性要求（CP3）

- **门禁 A（卡死）**：NPU 上 kernel 计算段在预算内结束；`SynchronizeStream` 返回；Host TRACE 约定编号闭合（见 KB §6）
- **门禁 B（正确）**：`c` 与 golden / liboqs（可用时）逐字节一致
- **门禁顺序**：A 未过不开性能刀；B 可在 A 稳定后启用（用户可要求提前，但不得用正确性捷径换挂死，见 cannbot「反模式」纪律）

---

## 5. ACLNN API

首期 **不定义** ACLNN；直调 host `main` + `run.sh` 验收。

---

## 6. 图模式 IR

不适用。

---

## 7. 性能要求

- 首期：**不设**利用率/带宽硬指标
- 仅设防挂死预算（各 `run.sh` 的 `KERNEL_COMPUTE_BUDGET_SEC`）
- CP4 性能迭代：**插件默认关闭**，卡死+正确后再议

---

## 8. 约束与要求

### 8.1 计算 / 同步（绑本仓 KB + cannbot CrossCore）

- CrossCore flag 字面量与生产契约：NTT **1/3**，GATE **4/8**，INTT **复用 1/3**；**禁止 5/7**
- **禁止** AIC 仍在 CrossCore Wait 时 `SyncAll`
- **禁止**自造 AIC↔AIV SoftSync；AIV↔AIV 若需汇合只跟已验 SoftSyncArrive 定式
- flagId ∈ 0–15；避开 SyncAll 保留区 **[11–14]**；若用 Matmul 高阶 API，须显式核算与其保留区冲突（cannbot `api-crosscore-sync`）
- **禁止**照抄旧 Encrypt 核内编排

### 8.2 资源

- MIX：`KERNEL_TYPE` / blockDim 在方案阶段锁定；不得为过编译擅自改已锁形状
- UB/L1 规划遵循 DAV_2201 MemBase 容量（`npu-arch`）

### 8.3 工程

- 一实验一目录；强制重建排除旧 bin 粘性
- 每刀编码后跑 cannbot `ascendc-sync-audit`
- 上机只打字回报 TRACE；不回传大文件

### 8.4 架构选型推荐（交 CP1 拍板）

| 候选 | 说明 | 推荐 |
|------|------|------|
| SIMD MemBase + MIX（AIC Cube + AIV Vec） | 与 910B3/B4（DAV_2201）一致；本仓 NTT/内积积木形态 | **推荐** |
| RegBase / SIMT | 主要面向更高代际（如 950）；非本芯片主路径 | 不推荐首期 |

---

## 9. CP1 请用户确认的问题

1. **架构**：是否批准「DAV_2201 · MemBase · MIX 直调」？
2. **落点**：新实现落 `graph-tests/enc_related/` 还是先 `$规格$` 后开 `examples/incubating/exp-…`？
3. **正确性节奏**：先卡死门禁再 liboqs，还是方案期就绑死 CP3 逐字节？
4. **Host μ**：默认 Host 折消息嵌入（对方 hangfree 图已采），还是设备内 PrefixEmbed？
5. **cannbot 安装**：是否授权把精选 skills **symlink 进** `.cursor/skills/vendor/cannbot-*`（会触 Rule）？是否 **禁止** 跑改写根 AGENTS 的 `init.sh`（推荐禁止）？
