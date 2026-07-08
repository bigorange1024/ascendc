# F203 KEM Alg.21 Decaps 设备全链与 SIM 单 session — 技术总结

**读者**：未参与本仓库开发的实现者 / Agent  
**目的**：说明 FIPS 203 **Algorithm 21** `ML-KEM.Decaps(dk, c)` 在 **ml_kem_1024（k=4）** 上的设备全链契约、FO 尾段边界，以及本轮发现的 **CAModel 单 session Decrypt→Encrypt 污染**诊断结论。  
**案例锚点**：[`ascendc-tests/fix-f203-alg21-kem-decaps-k4`](../../ascendc-tests/fix-f203-alg21-kem-decaps-k4/)（**单设备库合并版** · CPU 单 session PASS；SIM 默认 **2-session** PASS + 设备 FO；liboqs 分项 kat `CPU×10+SIM×1 PASS`）  

> **2026-07-02 更新（根因修正）**：本文早期把 SIM 单 session 重加密 `c'` 污染归为「泛化 CAModel 状态问题」。**实为探针曾用 decrypt/encrypt 双设备库**：一个 ACL session 内两份 device binary **func_key 空间重叠 / 装载边界冲突**，后加载库的核 launch 被派发到错误 binary。已由**合并单设备库**（单 func_key 空间）消除此双库冲突 —— 见 §4.3（含合库落地要点与 R3 触发面）与案例 STATUS「单库合并」节。
>
> **2026-07-08 更新（SIM host 选项统一）**：
> - SIM 生产默认改为 **`ASCENDC_SIM_HOST_MODE=decaps_2session`**（`run.sh` export；unset 等价）；排障 **`decaps_1session`**。
> - Host 判断：`ascendc::SimHostDecapsUse2Session()`（`ascendc_build_mode.hpp`）；**废弃**在新代码使用 `KEM_DECAPS_SIM_2SESSION`（头文件内临时兼容旧脚本）。
> - 全仓强制写法：[AscendC-CPU与SIM实现分叉开发指南.md](AscendC-CPU与SIM实现分叉开发指南.md) §4.1。
>
> **2026-07-03 更新（SIM 定论，纠正“死锁”误判）**：
> 1. **非死锁**：早期把 Phase-E 慢跑（Alg.7 rej 环活跃自旋 ~7min 无输出）误判为 hang，实为慢跑。
> 2. **SIM 默认 2-session 可靠**（`KEM_DECAPS_SIM_2SESSION=1`）：Phase-D `aclFinalize` 后 fresh session 跑 Phase-E + **设备 FO**（无 host memcmp）→ `K max=0`；`liboqs_kem_decaps_batch.sh` 逐轮换 `c` 对拍 `CPU×10+SIM×1 PASS`。
> 3. **单 session 首错在 `at_r5`**（`KEM_DECAPS_SIM_2SESSION=0`，排障用）：Phase-D 后 `m'/coins max=0`，Phase-E `c' max=244`，而 **PhaseE-only 对照 `K max=0`** → 系 **Phase-D 已执行触发的 CAModel session 级状态残留**（非 GM 输入 / 同步 / LUT / 算法错）。单 session 真修仍 open，2-session 为可靠保底。
> 4. **单库 SIM 构建坑**：`vendor/.../f203_alg7_rej_scalar.c` 是 CPU/参考语义文件，不参与设备热路径；若进 `ascendc_library`，AIC/AIV 合并阶段 `ld.lld -m aicorelinux` 报 `.c.o unknown file type`。修法：仅 CPU twin 库链入该 `.c`，SIM/NPU 设备库只保留 `.cpp` kernel 入口 + `.hpp` 内联逻辑（见 `cmake/decaps/CMakeLists.txt`）。
**讨论**：[`qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md`](../../qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) §7  
**实现方案**：[`INTEGRATION_PLAN.md`](../../ascendc-tests/fix-f203-alg21-kem-decaps-k4/INTEGRATION_PLAN.md)

---

## 0. 本文怎么读

| 章节 | 内容 | 是否依赖本仓库代码名 |
|------|------|----------------------|
| §1 | Alg.21 / Alg.18 数学与数据契约 | 否 |
| §2 | 设备全链不变量与 FO 边界 | 否 |
| §3 | AscendC 拼装模型 | 少量 |
| §4 | SIM 单 session 失败诊断方法 | 否 |
| §5 | 可复用模式与禁忌 | 否 |
| §6 | 案例附录：本探针首版状态 | 是 |

---

## 1. 数学与数据契约

### 1.1 参数集

本轮固定 **ml_kem_1024**：\(n=256\)，\(q=3329\)，\(k=4\)。Decaps 复用同参数集下已验收的 PKE Decrypt（Alg.15）与 PKE Encrypt（Alg.14）。

### 1.2 Algorithm 21 与 Algorithm 18

对外 `ML-KEM.Decaps(dk, c)` 调用内部 `Decaps_internal`。本仓实现时直接锁定 liboqs 展开密钥布局：

```text
dk_kem = dk_pke (1536) || ek (1568) || h=H(ek) (32) || z (32)
c      = K-PKE ciphertext (1568)
```

内部数据流：

```text
m'       ← K-PKE.Decrypt(dk_pke, c)
(K', r') ← G(m' || h)
c'       ← K-PKE.Encrypt(ek, m', r')
K        ← K'        if c = c'
          J(z || c)  otherwise
```

这里 `G` 为 SHA3-512，前 32B 是候选共享秘密 `K'`，后 32B 是重加密随机 coins。`J` 为拒绝路径的 SHAKE256 派生。

### 1.3 合法密文与拒绝路径

首版验收向量来自同一 `SEED_D=20260619` 下：

```text
Alg.19 KeyGen → dk_kem, ek
Alg.20 Encaps → c, K_enc
Alg.21 Decaps → K，应等于 K_enc
```

该向量只覆盖 **合法 `c` 路径**。拒绝路径必须额外构造篡改密文，证明 `c != c'` 时输出 `J(z||c)`，且不把 `K'` 暴露为最终输出。

---

## 2. 工程不变量

| 不变量 | 说明 |
|--------|------|
| **设备全链** | 生产路径中 `m'`、`G(m'||h)`、重加密、FO 比对与选择均应在 device 完成；Host 只负责 I/O 与验收对拍。 |
| **单 session 目标** | 目标编排为一次 `aclInit` / 一个 stream 串联 Decrypt → G → Encrypt → FO。 |
| **中间态不落盘** | 默认 `run.sh` 只产出 `output/K.bin`；`m'`、`K'`、`coins`、`c'` dump 仅用于非默认诊断。 |
| **Golden 只作 oracle** | alg20 `K.bin` / liboqs 只证明 I/O 等价，不规定 AscendC 内部实现形态。 |
| **自包含 vendor** | PKE 能力复制到本探针 `vendor/`，禁止默认路径跨探针 `#include` 或子进程调用其它 `run.sh`。 |

首版 SIM 暂时违反「单 session 目标」：为规避 CAModel 状态污染，合法密文路径采用两段 session。该例外是**阻塞记录**，不是新设计基线。

---

## 3. 平台实现模型

### 3.1 PKE 作为大段 vendor 能力

Decaps 的算力主体不是 KEM 尾段，而是：

| 阶段 | 来源 | 作用 |
|------|------|------|
| Phase-D | Alg.15 Decrypt vendor | `dk_pke + c → m'` |
| K1 | 新增 KEM 设备逻辑 | `G(m'||h) → K', r'` |
| Phase-E | Alg.14 Encrypt G5 vendor | `ek + m' + r' → c'` |
| K2 | 新增 KEM FO 逻辑 | `c` vs `c'`，选择 `K'` 或 `J(z||c)` |

因此调试要先把 PKE 两端当作已验收黑盒，只在接口张量上定位问题：`m'`、`K'`、`coins`、`c'`、`K`。

### 3.2 FO 尾段应靠近 pack

理想位置是 Encrypt 最后 `pack` 生成 `c'` 后立即执行 FO：

```text
pack 写 c' → 比对输入 c → 计算 J(z||c) → 常数时间选择 → K
```

这样 `c'` 不必 D2H，不需要额外生产输出，也能把拒绝路径绑定到真实设备数据流。

### 3.3 `func_key` 与分库

Alg.14 G5 与 Alg.15 G4 本身已经接近 CAModel AIV-only `func_key` 边界。Decaps 再新增 K1/K2 时，要把「核入口数量」视为资源预算：

- 优先嵌入已有尾段；若独立 AIV 核便于调试，必须补 `nm` 审计。
- decrypt / encrypt 分库可规避编译期头重定义，但也会改变 SIM 装载边界；不能把“分库能编译”当作“单 session 稳定”。
- `KERNEL_FILES` 中存在但本模式不 launch 的 entry 仍可能消耗编译与装载资源。

---

## 4. SIM 单 session 失败诊断方法

### 4.1 现象分层

本轮失败不是“一跑全链 K 错”这么粗粒度，而是按接口张量拆成：

```text
m'       max=0   ✓
K'/coins max=0   ✓
c'       max=244 ✗
K        max=216 ✗
```

这说明 Decrypt 与 K1 已正确，FO 输出错是因为重加密 `c'` 错，随后合法密文被误判为拒绝路径。

### 4.2 排除随机性问题

判断 `coins` 是否正确，比直接看 `K` 更关键：

| 检查 | 结论 |
|------|------|
| `m'` vs host / alg15 期望 | max=0 |
| `G(m'||h)` 的 `K'` | max=0 |
| `G(m'||h)` 的 `coins` | max=0 |
| 同组 `m'/coins` 单独跑 alg14 G5 SIM | `c'` max=0 |

若上述成立，则不应继续改 `SEED_D`、`h` 切片或 SHA3；问题在 Phase-E 所处的 session 环境。

### 4.3 判断是 CAModel session 状态问题

本轮尝试过在同一 session 内释放 decrypt GM、重载 encrypt LUT，`c'` 仍错；而 `aclFinalize` 后 fresh alg14 G5 session 可恢复 `c' max=0`。

可复用结论（2026-07-02 定论）：

> 当 A、B 两段各自 SIM PASS，A→B 单 session 下 B 的输入 dump 正确但输出错，**首先检查是否 A、B 分属两个设备 `.so`**。CAModel 一个 ACL session 只登记一份「活跃」device binary / func_key 空间；两份设备库同 session 时，后加载库的核 launch 会被派发到错误 binary → 输出形状对值全错（本例 `c' max=244`），fresh session 只 launch 一侧则恢复。**修法：合并单设备库**（单 func_key 空间）。这也解释了为何本仓所有过关 SIM 探针都是单库单 session。

**合库落地要点**（本案例）：
- 双 vendor 树同名头逐个 `diff`：本例 21 个同名头**仅 `aiv_func.hpp` 内容分歧**（NTT-forward vs NTT+INTT），其余 20 个逐字节相同。分歧头必须改名（decrypt→`dec_aiv_func.hpp`），否则单一 `-I` 路径下裸名 `#include` 会拉错树 → `namespace tiling` 重定义（CPU 编译即暴露，是可 CPU 复现的护栏）。
- 合库后 AIV-only func_key 会累加：本例 5（`kem_dec_g`+encrypt 侧 4）触 R1（≥5→507000），把数据/哈希通路核 `kem_dec_g` 改 **MIX_AIC_1_2 占位**回落 4。
- vendor 由 sync 脚本 `rsync --delete` 覆盖时，改名须写进 sync 脚本**每次重放**（幂等），否则重建即被还原。

该结论把既有 [`AscendC-CAModel-SIM-funckey与单session约束知识库.md`](AscendC-CAModel-SIM-funckey与单session约束知识库.md) 的 R1/R2 补上第三条触发面 **R3：一个 session 多个设备 `.so`**（无错误码、后段输出污染）。

---

## 5. 可复用模式

| 模式 | 做法 | 禁忌 |
|------|------|------|
| **P1：接口张量切分** | 先验 `m'`、`K'/coins`、`c'`，最后看 `K` | 只盯最终 `K`，导致把 FO、Encrypt、SHA3 混在一起猜 |
| **P2：合法路径与拒绝路径分开验** | 合法 `c` 证明 `K'` 路径；篡改 `c` 单独证明 `J` 路径 | 用合法向量 PASS 宣称 FO 完整 |
| **P3：PKE vendor 黑盒复验** | 同一接口张量单独跑上游 PKE 段 | 把已验 PKE 源码当成可随手改的调试区 |
| **P4：SIM workaround 明确降级** | workaround 写入方案、qa、note，标“有条件完成” | 把两段 session 当作新生产基线 |
| **P5：Host 只做诊断，不做密码学替代** | Host `memcmp` 可用于 SIM 合法路径阻塞绕行 | Host 计算 `J` 或默认路径替代设备 FO |

---

## 6. 附录：本探针首版状态

### 6.1 参数与验收

| 项 | 值 |
|----|-----|
| 探针 | `ascendc-tests/fix-f203-alg21-kem-decaps-k4` |
| 输入 | alg19 `dk_kem.bin` + alg20 `c.bin`，`SEED_D=20260619` |
| CPU | G4 合法 `c` 路径 PASS，单 session + 设备 FO；拒绝路径（篡改 device `coins[0]`）`K=J(z‖c)` PASS |
| SIM | G4 合法 `c` 路径 PASS，默认 **2-session + 设备 FO**（无 host memcmp） |
| SIM tick | Phase-D 约 534k + fresh Phase-E 约 899k |
| liboqs 分项 kat | `liboqs_kem_decaps_batch.sh` 逐轮换 `c` → `K max=0`，`CPU×10+SIM×1 PASS` |
| 未完成 | 单 session SIM 真修（`at_r5` 首错）、`nm` func_key 审计、拒绝路径 SIM 长测、NPU 实机 |

### 6.2 SIM 默认 2-session（设备 FO，非 host workaround）

```text
session-1: Decrypt + G → m', K', coins           # Phase-D
aclFinalize
session-2: fresh run_g5_sim_full(ek, m', coins)  # Phase-E 重加密 + 设备 FO
           → 设备 f203_kem_dec_pack(KemDecFo): c vs c' 常数时间选择 → K
```

与首版 host `memcmp` workaround 不同：**FO 比对与选择在设备完成**（`KemDecFo`），SIM 2-session 只是把单 session 拆成两段以规避 Phase-D→Phase-E 的 CAModel session 级状态残留（单 session `at_r5` 首错）。拒绝路径（篡改 device `coins[0]` → `c'≠c` → `J(z‖c)`）CPU 已 PASS，SIM 2-session 架构相同。

### 6.3 后续关闭条件

本 note 中的 SIM 例外只有在满足以下任一条件后才能关闭：

1. 恢复单 session D→G→E→FO，CPU+SIM 合法路径与拒绝路径均 PASS。
2. 若 CAModel 9.0 确认无法承载该超长单 session，保留两段 session 但新增独立设备 FO SIM 单测，明确生产 NPU/CPU 路径仍为单 session。

---

## 7. 相关文档

| 文档 | 作用 |
|------|------|
| [`F203-KEM-Alg19-KeyGen设备全链技术总结.md`](F203-KEM-Alg19-KeyGen设备全链技术总结.md) | KEM 展开密钥、设备 SHA3 与 `dk_kem` 布局 |
| [`F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](F203-PKE-liboqs交叉验证与Compress定点技术总结.md) | PKE 与 liboqs oracle、`Compress_d` 定点契约 |
| [`F203-Alg15-Decrypt-2launch编排技术总结.md`](F203-Alg15-Decrypt-2launch编排技术总结.md) | Decrypt 2-launch 编排边界 |
| [`AscendC-CAModel-SIM-funckey与单session约束知识库.md`](AscendC-CAModel-SIM-funckey与单session约束知识库.md) | CAModel `func_key` 与同步约束 |
