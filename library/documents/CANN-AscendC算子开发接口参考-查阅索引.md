# CANN Ascend C 算子开发接口参考 — 查阅索引

**对应 PDF**：[CANN社区版 9.0.0 Ascend C算子开发接口参考 01.pdf](CANN社区版%209.0.0%20Ascend%20C算子开发接口参考%2001.pdf)  
**文档版本**：01（2026-05-27）  
**用途**：PDF 体积大（约 27MB），本文件记录**查阅过的主题**在 PDF 中的**章节 / 页码**及**简要概括**，便于后续快速定位。每次在对话或开发中查阅该 PDF 的 API / 约束 / 架构相关内容后，应**追加一条**「查阅记录」。

**离线网页**：历史曾用 `library/offline-web/` 归档社区站；**当前仓库未收录**，以本目录 PDF + 本查阅索引为准。

---

## 查阅工作流（强制）

写码或设计用到 AscendC API 时，**按序**执行：

1. **先查本索引**：在下方「查阅记录」与「常用子节」表中搜索 API 名；有记录则按 PDF 页码 / 在线链接与「概括」列实现。
2. **索引无记录**：查 [CANN社区版 9.0.0 Ascend C算子开发接口参考 01.pdf](CANN社区版%209.0.0%20Ascend%20C算子开发接口参考%2001.pdf)（可用 `pdftotext` + 目录页码，或社区在线 API 页）。
3. **查完必写回**：在「查阅记录」**顶部**追加一行（日期、主题、章/节/页、约束与用法概括）；避免后续重复翻 PDF。

**审查红旗**：实现里用了 `Compares`/`GatherMask`/`GetCmpMask` 等，但索引与 git 历史均无对应查阅记录。

---

## PDF 顶层目录（页码摘自 PDF 目录页）

| 章 | 标题 | 起始页 |
|----|------|--------|
| 1 | Ascend C API 列表 | 1 |
| 2 | **SIMD API**（本项目当前采用） | 53 |
| 3 | SIMT API（暂不采用） | 2345 |
| 4 | Utils API | 2954 |
| 5 | AI CPU API | 3147 |
| 6 | 附录 | 3153 |

### 第 2 章 SIMD API — 常用子节

| 节 | 标题 | 起始页 |
|----|------|--------|
| 2.1 | 通用说明和约束 | 53 |
| 2.2 | 基础数据结构 | 57 |
| 2.3 | 基础 API | 136 |
| 2.3.1 | Memory 数据搬运 | 136 |
| 2.3.2 | 矩阵计算（ISASI） | 216 |
| 2.3.3 | Memory 矢量计算 | 357 |
| 2.3.3.4 | **比较与选择**（Compare / Compares / Select / GatherMask 等） | 549 |

---

## 查阅记录

按时间倒序追加（最新在上）。

| 2026-07-27 | **ML-KEM-512 W1 B5 Stage123 NTT/INTT polyvec4 k2 探针** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`TPipe`/`TQue`/`TBuf`/`GlobalTensor`/`LocalTensor`/`GetValue`/`SetValue`/`PipeBarrier`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast`/`CrossCore*` 均复用既有记录；Stage1–3 禁 `Gather`，true polyvec4 不新增 AscendC API | [`pass-fix-f203-stage123-ntt-intt-polyvec4-k2`](../../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-stage123-ntt-intt-polyvec4-k2/)：MIX `blockDim=1`；逻辑 `m=8` 由 Cube 硬件 pad 到 16（非假 poly）；AIV 连续 `{0,1}`/`{2,3}`；NTT/INTT 双模式 |
| 2026-07-27 | **ML-KEM-512 W1 B6 MultiplyNTTs + Inner k2 探针** | `GetBlockIdx`/`DataCopy`/`TPipe`/`TQue`/`TBuf`/`GlobalTensor`/`LocalTensor`/`Add`/`PipeBarrier` 均复用既有记录；Alg.11 ROM LUT 为 `__gm__` 常量 + UB `DataCopy` 已登记；k2 1+1 分片不新增 AscendC API | [`pass-fix-f203-alg11-12-multiply-inner-k2`](../../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg11-12-multiply-inner-k2/)：`multiply/` 单对 Alg.11；`inner/` `P_OUT=S_VEC=2`，AIV0 `t_hat[0]` / AIV1 `t_hat[1]` |
| 2026-07-27 | **ML-KEM-512 W1 B4 SampleNTT k2 探针** | `GetBlockIdx`/`DataCopy`/`TPipe`/`TBuf`/`GlobalTensor`/`LocalTensor`/`GetValue`/`SetValue`/`PipeBarrier`/`Mins`/`Gather`/shared Keccak 均复用 2026-06-23 Alg.7 与既有记录；k2/2×2 不新增 AscendC API | [`pass-fix-f203-alg7-sample-ntt-k2`](../../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg7-sample-ntt-k2/)：`SEED_D`→`...-k2:SEED_D=`→`G(d||2)`；单 poly `(j,i)`；矩阵验收 `{0,1}²` |
| 2026-07-27 | **ML-KEM-512 W0 B3a CBD-η2 k2 探针** | `GetBlockIdx`/`GetBlockNum`/`DataCopy`/`TPipe`/`TQue`/`TBuf`/`GlobalTensor`/`LocalTensor`/`GetValue`/`SetValue`/`PipeBarrier` 均复用既有记录；η2/k2 不新增 AscendC API | [`pass-fix-f203-alg8-cbd-eta2-k2`](../../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg8-cbd-eta2-k2/)：`prf_out[4,128]` → `src[4,256]`；AIV0 `{0,2}` / AIV1 `{1,3}`；SIM `blockDim=2` |
| 2026-07-26 | **ML-KEM-768 E20 KEM Encaps customspec** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`Duplicate`/`PipeBarrier`/`CrossCore*`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast`/`Gather` 均复用既有记录；KEM head `Sha3OneShot` 走 shared Keccak，非新增矢量 API；NTT S1–S3 禁 `Gather` | [`exp-fips203-mlkem-kem-encaps-k3-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-encaps-k3/exp-fips203-mlkem-kem-encaps-k3-实现方案-customspec.tex)：SIM 2 launch；ek=1184，m=32 → c=1088，K=32；复用 E14/D20 k3 几何，KEM head 并入 prep |
| 2026-07-26 | **ML-KEM-768 E21 KEM Decaps customspec** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`Duplicate`/`PipeBarrier`/`CrossCore*`/`SyncAll`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast`/`Gather` 均复用既有记录；KEM head/tail `Sha3OneShot`/`Shake256OneShot` 走 shared Keccak，非新增矢量 API；NTT S1–S3 禁 `Gather` | [`exp-fips203-mlkem-kem-decaps-k3-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-k3/exp-fips203-mlkem-kem-decaps-k3-实现方案-customspec.tex)：D21 delivery；SIM 默认 `decaps_1session`；dk=2400、c=1088 → K=32；Phase-D=E15/D15，Phase-E=E14/D14+FO |
| 2026-07-26 | **ML-KEM-768 E21ct KEM Decaps CT customspec** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`Duplicate`/`PipeBarrier`/`CrossCore*`/`SyncAll`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast`/`Gather` 均复用 E21/D21ct 已查记录；KEM head/tail `Sha3OneShot`/`Shake256OneShot` 走 shared Keccak，非新增矢量 API；NTT S1–S3 禁 `Gather` | [`exp-fips203-mlkem-kem-decaps-ct-k3-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-ct-k3/exp-fips203-mlkem-kem-decaps-ct-k3-实现方案-customspec.tex)：D21ct CT；SIM 默认 `decaps_2session`；dk=2400、c=1088 → K=32；accept/reject 必验且 reject≠accept |
| 2026-07-26 | **ML-KEM-768 E19 KEM KeyGen customspec** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`Duplicate`/`PipeBarrier`/`CrossCore*`/`SyncAll`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast` 均复用既有记录；KEM tail `Sha3OneShot` 走 shared Keccak，非新增矢量 API；NTT S1–S3 禁 `Gather` | [`exp-fips203-mlkem-kem-keygen-k3-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-keygen-k3/exp-fips203-mlkem-kem-keygen-k3-实现方案-customspec.tex)：2 launch；ek_kem=1184、dk_kem=2400；复用 E13/D19 k3 几何，Alg.16 tail 内嵌第二 launch |
| 2026-07-26 | **ML-KEM-768 E15 PKE Decrypt customspec** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`Duplicate`/`PipeBarrier`/`CrossCore*`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast` 均复用既有记录；NTT S1–S3 禁 `Gather`；k3 D15 fused 几何锁定 | [`exp-fips203-mlkem-pke-decrypt-k3-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-decrypt-k3/exp-fips203-mlkem-pke-decrypt-k3-实现方案-customspec.tex)：1 fused MIX launch；dk/c/m=1152/1088/32，du/dv=10/4；NTT/INTT k3 AIV 2+1；tail Compress1+ByteEncode1 |
| 2026-07-26 | **ML-KEM-768 E14 PKE Encrypt customspec** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`PipeBarrier`/`CrossCore*`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast`/`Gather` 均复用既有记录；NTT S1–S3 禁 `Gather`；k3 D14 几何锁定 | [`exp-fips203-mlkem-pke-encrypt-k3-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-encrypt-k3/exp-fips203-mlkem-pke-encrypt-k3-实现方案-customspec.tex)：2 launch；prep AIV `blockDim=2`（Â 5+4 + `re[7]`）；compute MIX `blockDim=1`；ek/c=1184/1088，du/dv=10/4，INTT batch4 |
| 2026-07-26 | **ML-KEM-768 E13 PKE KeyGen customspec** | `GetBlockIdx`/`GetSubBlockIdx`/`DataCopy`/`PipeBarrier`/`CrossCore*`/`Mmad`/`Fixpipe`/`ShiftRight`/`Muls`/`Add`/`Sub`/`Cast`/`Gather` 均复用既有记录；NTT S1–S3 禁 `Gather`；k3 D13 几何锁定 | [`exp-fips203-mlkem-pke-keygen-k3-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-keygen-k3/exp-fips203-mlkem-pke-keygen-k3-实现方案-customspec.tex)：2 launch；prep AIV `blockDim=2`（Â 5+4 + polyvec6）；compute MIX `blockDim=1`；ek/dk=1184/1152 |
| 2026-07-20 | **SyncAll（Decaps T19i FO 尾）** | §2.3.7.2.3 p.1086–1088；`SyncAll<isAIVOnly=true>()`；MIX 下 AIC 已返回后双 AIV 汇合；pack 分片写完再 FO | [`pass-fix-…-decaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/) `kem/f203_encrypt_l18_l19_kernel.cpp`：`tail_pack_shard_gm` 后 SyncAll，AIV0 `KemDecFo`；SIM 4→3 |
| 2026-07-18 | **Alg.21 KEM Decaps customspec（exp）** | `GetBlockIdx`/`DataCopy`/`PipeBarrier`/`CrossCore*`（复用 Decrypt/Encrypt/Encaps 已查）；头段 `Sha3OneShot` G、FO 段 `Shake256OneShot` J（shared Keccak，非矢量 API）；NTT S1–S3 **禁 Gather** | [`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-k4/exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex)：SIM 4/CPU 6；`dk_kem`+`c`→仅 `K`；基线 `pass-fix-f203-alg21-kem-decaps-device-k4`（D**286803**+E**745925**；单库+`decaps_1session`） |
| 2026-07-17 | **Alg.21 Decaps 全链（Phase-D+E）** | Decrypt fused：复用 07-09 已查 CrossCore/GATE/`DataCopy`；Encrypt/G/FO：复用 Encaps 记录；无新矢量 API | [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/)：CPU 单库；SIM 双库+2-session；`K` max=0；D**283317**+E**745341** |
| 2026-07-17 | **Alg.21 Decaps Phase-E（G + FO）** | `GetBlockIdx`（复用 Encaps）；头段 `Sha3OneShot` G；尾段 `Shake256OneShot` J；无新矢量 API；NTT S1–S3 **禁 Gather** | [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/)：prep 前段 G；CPU pack+FO / SIM `fo_only`；合法路径 tick **746221** |
| 2026-07-15 | **Alg.20 KEM Encaps customspec（exp）** | `GetBlockIdx`/`DataCopy`/`PipeBarrier`/`CrossCore*`（复用 Encrypt 已查）；头段 SHA3 走 shared Keccak 非矢量 API；NTT S1–S3 **禁 Gather** | [`exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4/exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex)：SIM 2/CPU 5；ek+m→c+K；基线 `pass-fix-f203-alg20-kem-encaps-device-k4`（tick 721010） |
| 2026-07-15 | **Alg.20 Encaps device 头 SHA3** | shared `Sha3OneShot`（非矢量 API）；`GetBlockIdx` | [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/)：prep 前段 `H=SHA3-256`/`G=SHA3-512`；仅 block0；Encrypt 余路径引用 stable |
| 2026-07-13 | **SyncAll（硬同步，AIV-only）** | §2.3.7.2.3 p.1086–1088；`SyncAll<isAIVOnly=true>()` 硬同步无 GM workspace；A2 支持硬同步；`isAIVOnly=true` 仅汇合 Vector 核（MIX 下 AIC 已返回时可用） | KEM KeyGen Fuse/Tail 前双 AIV 汇合，避免 AIV0 拷未写完的 `sk_out` 末 poly |
| 2026-07-13 | **Alg.19 KEM KeyGen customspec（exp）** | `DataCopy`/`GetSubBlockIdx`/`CrossCore*`/`ShiftRight`/`Muls`（复用 KeyGen 已查）；尾段 SHA3 走 shared Keccak 非矢量 API；NTT S1–S3 **禁 Gather** | [`exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4/exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.tex)：2 launch；ek\_kem 1568 / dk\_kem 3168；基线 `pass-fix-f203-alg19-kem-keygen-device-k4` |
| 2026-07-10 | **统一整数 Compress limb 向量** | `Muls`/`Adds`/`Add`/`ShiftRight`；C=629·2^16+63213；carry 进位链 | [`f203_unified_compress_vec.hpp`](../../library/shared/f203_unified_round/f203_unified_compress_vec.hpp)：纯 int32 向量宽乘；`pass-f203-compress-unified-int-vec-k4` 全 d CPU+SIM PASS |
| 2026-07-09 | **Decrypt 单 kernel pad/ŝ：禁标量写 GM** | `Duplicate`/`DataCopy`（既有）；Encrypt R2 | [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/)：`pad_w_hat_for_intt`、`decode_s_hat` 改 UB+DataCopy；softSyncGm 仅 AIV0/1 汇合；GATE 4/8。 |
| 2026-07-09 | **Decrypt 单 kernel + v−w 向量 mod** | CrossCore flag 1/2/3 + GATE 4/8（Encrypt l18_l19 同构）；`Sub`/`Max`/`ShiftRight` wrap_mod | [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/) `f203_decrypt_device_fused`：prep→NTT→dot→INTT→尾；除 Encode₁ 外向量。 |
| 2026-07-09 | **Decrypt prep Decompress₁₁/₅ 向量** | `Muls`/`Adds`/`ShiftRight`（P-DEC，既有） | [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/) unpack：ByteDecode 标量 + Decompress 向量；生产 **仅 D2H m**。 |
| 2026-07-09 | **Compress₁ 向量 Barrett（Decrypt 尾）** | `Muls`/`Adds`/`ShiftRight`（07_0055 / 07_0059，既有记录） | [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/)：d=1 `Muls(1290168)`+`Adds(1<<30)`+`ShiftRight(31)`→{0,1}；int32 环绕≡u32 Barrett；Encode₁ 标量。对齐 liboqs；与旧 G4 `(Q+1)/2` 在 u=832 差 1 bit。 |
| 2026-07-09 | **Alg.14 PKE Encrypt customspec（exp-fips203-mlkem-pke-encrypt-k4）** | DataCopy/CrossCore/MMAD/向量算术/GatherMask/Cast；NTT S1–S3 **禁 Gather** | [`exp-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4/exp-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.tex)：I/O 仅 ek+m+coins→c；SIM 2 / CPU 5 launch；中间态禁落盘；API 表复用 KeyGen/Encrypt 探针已查记录 |
| 2026-07-08 | **ByteEncode_d d=5/d=11 真·向量 pack 实验** | `Gather`/`CreateVecIndex`/`Muls`/`ShiftRight`/`Add`（[07_0059](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0059.html) ShiftRight；Gather 见 07_0071 邻域）| `pass-f203-byteencode-d-vec-k4` `BYTE_ENCODE_D_VEC=2`：`Gather(dst,src,offsetU32(字节偏移),0,count)` 按 gg*32+k*4 去交错取每组 8 个 position-lane；`CreateVecIndex(idx,0,n)`+`Muls` 造字节偏移。**结论**：d=5/d=11 拼字非字节对齐、拼字仍需标量 GetValue，真·向量 pack 比标量逐组 **更慢**（d=5 5839 vs 5464；d=11 7404 vs 6604 tick），不采纳；d=12 可向量因 2×12=24bit=3B 对齐 |
| 2026-07-08 | **Compress_d d=4/5/10/11** | Cast/Div；Barrett int32 | `pass-f203-compress-d-vec-k4`：d=4/5 Barrett；d=10/11 cast_div；探针目录更名 |

### 2026-06-23 — Alg.7 SampleNTT rej 向量 compact / 比较掩码链（pass-fix-f203-alg7-sample-ntt-k4）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **探针背景** | — | [`ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg7-sample-ntt-k4/)：rej 后 stream compact 取前 256 个 â；R5 草稿误用 `Compares(LT/NE)` + `GetValue(dst)!=0` 导致 SIM 失败，根因是**未按本索引/PDF 查 API**。 |
| **Compares** | 2.3.3.4.3，**p.558**；[07\_0068](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0068.html) | 逐元素与**标量**比较；`dst` 为 **`uint8_t` bit 打包**（小端，每 bit 对应 src0 一元素），**不是**每 lane 一字节 0/1，禁止 `GetValue(b)!=0` 判真。A2：`half/float` 支持全部 `CMPMODE`；**`int32_t` 仅 `CMPMODE::EQ`**（与 `Compare` 同，见 2026-05-19 条）。Level-1 `count` 接口：**count 个元素所占空间须 256B 对齐**（int32 常用 count=128）。`dst`/`src0` 起始地址 **32B 对齐**。 |
| **Compare（结果存入寄存器）** | 2.3.3.4.2，**p.554**；[07\_0067](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0067.html) | 双 tensor 比较，结果写入 **CmpMask 寄存器**（非 `dst` LocalTensor）；配合 **`GetCmpMask`** 读出。float/half 上 `LT` 等可用；int32 仅 EQ（A2）。compact 备选：**`Compare` + `GetCmpMask`** 得掩码再 `GatherMask`。 |
| **GetCmpMask(ISASI)** | 2.3.3.4.5，**p.568**；[07\_0223](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0223.html) | 读取 **`Compare`（寄存器版）** 后的 CmpMask；`dst` 为 `uint8_t` LocalTensor，**≥128B**，**16B 对齐**。示例：float `Compare(LT)` 后 `GetCmpMask` 得按 bit 展开的掩码字节。与 **`Compares` 的 `dst` LocalTensor** 是不同路径。 |
| **SetCmpMask(ISASI)** | 2.3.3.4.6，**p.570** | 为**不传 mask 的 `Select`** 预设比较寄存器；`SELMODE::VSEL_CMPMASK_SPR` 等。与 `GetCmpMask` 对称。 |
| **Select** | 2.3.3.4.7，**p.572**；[07\_0070](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0070.html) | 按 **`selMask` 比特**：1→取 `src0`，0→取 `src1`（或标量模式 1）。A2 支持模式 0/1/2。rej **`vec_mask` 路径**：`Compares(EQ)` 得 bit 掩码后，可用 **`Select`** 在 stream 与 0 间选路（需正确理解 `selMask` 布局）。高阶封装另见 [07\_0859](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0859.html)。 |
| **GatherMask** | 2.3.3.4.9，**p.588**；[07\_0071](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0071.html) | 按 **gather mask**（内置模式或用户 `src1Pattern` Tensor）从 `src0` **收集**元素写入 `dst`；`rsvdCnt` 输出保留元素个数。A2：`reduceMode=true` **Counter 配置方式一**（每 repeat `mask` 个元素）。`src1Pattern` 类型随 `T`：float/int32 用 `uint32_t` 等。compact 方向：**掩码 bit / `GetCmpMask` 结果 → `GatherMask` 压紧**，分 tile 累加至 256。 |
| **Mins** | 2.3.3.1.24，**p.437**；[07\_0057](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0057.html) | 逐元素与标量取 min：`dst[i]=min(src[i], scalar)`。Alg.7 rej **`vec_mins`（默认）**：`int16` stream 与 `q` 标量 `Mins` 实现 `<q` 剔除（等价 `Compares(LT)` 但无 bit 掩码）。 |
| **Compare / CompareScalar（对照）** | 见 2026-05-19 条；[07\_0066](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0066.html) | A2 **`int32` 仅 EQ**；`GE/LT` 需 float/half 链。`Compares` 与 `Compare` 约束一致，但输出形态不同（LocalTensor bit 包 vs 寄存器）。 |

**R5 compact 正确路线（待实现）**：`int16` 上 `Mins` 已用于剔除；compact 应用 **`Compare(EQ)` 或 float 链 `Compare(LT)` + `GetCmpMask`**，或 **`Compares`（仅 EQ on int32）** 生成掩码，再 **`GatherMask`** 分块前缀压紧；**禁止** int32 `Compares(LT/NE)` 与把 `Compares` 的 `uint8` dst 当逐 lane 布尔读。

### 2026-06-19 — Alg.13 行 16–20 2s1e UB 融合 customspec（FIPS CBD 采样）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **2s1e Alg13 customspec** | — | [`exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4-实现方案-customspec.tex)：行 16–20；MIX `blockDim=1`；平面 mat\_c；UB 融合；**Host FIPS SamplePolyCBD s/e**（禁止 FIXED\_POLY）。 |
| **CrossCore / DataCopy / Add** | 见 Stage12、Stage1 查阅记录 | 与 vec-k4-v2 同构；NTT S1–S3 无 Gather。 |
| **Alg.11 Gather** | 2.3.3 | 仅行 18 basemul；非 NTT 平面读。 |

### 2026-05-19 — F203 Stage1+2 融合 customspec（MIX、aicore=1）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **Stage12 customspec** | — | [`exp-mlkem-f203-stage12-encode-matmul-mix-实现方案-customspec.tex`](../../examples/frozen/frozen-exp-mlkem-f203-stage12-encode-matmul-mix/exp-mlkem-f203-stage12-encode-matmul-mix-实现方案-customspec.tex)（**已 frozen**，原 incubating 路径已关闭）：Stage1 encode + Stage2 Matmul；`KERNEL_TYPE_MIX_AIC_1_2`；`blockDim=1`；`CrossCore*` S1→S2。 |
| **CrossCoreSetFlag / CrossCoreWaitFlag** | API 列表 $\approx$p.15–16 | S1 AIV→AIC、S2 AIC→AIV pack；参考 [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/mmad_custom.cpp)。 |
| **Matmul / REGIST_MATMUL_OBJ** | Matmul 教程；Stage2 隔离用例 | `int8×int8→int32`；`aicore=1` tiling 闭合。 |
| **Stage1 向量 API** | 见 Stage1 customspec 查阅记录 | encode 主路径 100\% 向量。 |

### 2026-05-19 — F203 Stage1 纯向量 / Stage3 向量预研（Cast、算术、搬运、比较）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **Stage1 customspec** | — | [`exp-fips203-mlkem-pke-stage1-encode-vec-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-stage1-encode-vec/exp-fips203-mlkem-pke-stage1-encode-vec-实现方案-customspec.tex)：数学 → 分块 → 全量 API 表 → 逐步覆盖率（主路径 **100% 向量**）。 |
| **Stage3 customspec** | — | [`exp-fips203-mlkem-pke-stage3-routea-mod-vec-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-stage3-routea-mod-vec/exp-fips203-mlkem-pke-stage3-routea-mod-vec-实现方案-customspec.tex)：RouteA+mod；**int64 合并与 mod 标量缺口**（Div 无 int32、int64 无矢量算术）。 |
| **Cast** | 2.3.3；[07\_0073](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0073.html) | A2 **表 6**：无 `int32→int8` / `int16→int8`。910B 实测链：`int32→int16→half→int8`（`CAST_NONE`）。dav\_c220 `CheckCastDatatype` 与表一致。 |
| **ShiftRight** | 2.3.3；[07\_0059](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0059.html) | `int32_t` 算术右移；A2 scalar∈[0,32]。Stage1：`hi=v>>6`。 |
| **Muls** | 2.3.3；[07\_0055](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0055.html) | A2：`half/float/int16_t/int32_t`。Stage1：`hi*64`；Stage3 预研：`×4096/×64`（int32 有溢出风险）。 |
| **Sub** | 2.3.3；[07\_0036](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0036.html) | A2：`half/int16_t/int32_t/float`。Stage1：`lo=v-hi*64`。 |
| **Add / Mul** | 2.3.3；[07\_0035](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0035.html) / [07\_0037](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0037.html) | A2 支持 `int32_t`。Stage3：`hl+lh` 等（受 int64 合并约束）。 |
| **Div** | 2.3.3；[07\_0038](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0038.html) | 文档面向 half/float；**dav\_c220 仅 half/float**（`kernel_operator_vec_binary_impl.h`）。Stage3.1 **不能**直接 int32 向量除法。 |
| **Compare / CompareScalar** | 2.3.3；[07\_0066](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0066.html) | A2：`int32_t` **仅 CMPMODE::EQ**；`GE/LT` 等需 float/half。mod 双校正比较需标量或 float 链。 |
| **And** | 2.3.3；约 07\_0062 | A2：**int16_t/uint16_t**，非 int32。Stage1 未采用 `v&63`。 |
| **DataCopy** | 2.3.1；[07\_0101](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0101.html) | GM↔UB 连续块；Stage1/3 主搬运。 |
| **DataCopyPad** | 2.3.1 ISASI；[07\_0265](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0265.html) | A2 支持 `int32`；跨步 gather **备选**（Stage3 未用）。 |
| **Select** | 高阶；[07\_0859](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0859.html) | 掩码选路；mod 向量备选需配合 Compare。 |
| **TPipe / InitBuffer / TQue / TBuf** | 2.3 资源管理；[07\_0108](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0108.html) / [07\_0110](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0110.html) / [07\_0137](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0137.html) | 双缓冲流水；`TBuf` 作 `VECCALC`。 |
| **GlobalTensor / LocalTensor** | 2.2；[07\_0007](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0007.html) / [07\_0006](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0006.html) | `SetGlobalBuffer`；`GetValue`/`SetValue` 用于 Stage3 标量 tile。 |
| **GetBlockIdx** | 2.3 系统变量；[07\_0185](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0185.html) | 8 核各绑 1 poly。 |
| **KERNEL\_TASK\_TYPE\_DEFAULT** | Utils / 编程指南 | `KERNEL_TYPE_AIV_ONLY` 纯向量 Stage1/3。 |

### 2026-06-28 — F203 Encrypt 侧 Compress / Decompress / ByteEncode_d 探针

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **F203 Compress_d** | [`pass-f203-compress-d-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-compress-d-vec-k4/) | **d=4/10 PASS**；d=4：`Muls`+`Adds`+`ShiftRight`+`mask_low_bits`；d=10 标量 u64 Barrett。ml_kem_1024 **d=5/11** 见 Encrypt pack。 |
| **F203 Decompress_d** | [`pass-f203-decompress-d-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-decompress-d-vec-k4/) | **d=4/10 PASS**；`Muls(Q)`+`Adds(bias)`+`ShiftRight(d)`。 |
| **F203 ByteEncode_d** | [`pass-f203-byteencode-d-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-byteencode-d-vec-k4/) | **d=4/5/10/11 PASS**；256-wide `mask_low_bits_i32`；分组标量 pack（8 coeff/组 for d=5/11）。 |
| **F203 ByteDecode_d** | [`pass-f203-alg6-bytedecode-d-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-alg6-bytedecode-d-vec-k4/) | **d=4/5/10/11 PASS**；d=4 向量 nibble；d=5/10/11 分组 unpack。 |
| **mask_low_bits 模板** | 见 Stage1、`byte_encode12_vec.hpp` | `v mod 2^bits`：`ShiftRight(v,bits)` → `Muls(·,2^bits)` → `Sub(v,v,·)`；A2 支持 int32。 |
| **GetValue / SetValue** | [07\_0006](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0006.html) | ByteEncode_d pack 阶段：nibble / 10-bit 交织写 `uint8` 输出（32/64 组循环）。 |

### 2026-06-09 — 项目平台基线与文档入口（初始化）

| 查阅主题 | PDF 位置 | 概括 |
|----------|----------|------|
| **开发工具链版本** | —（项目约定，非 PDF 单节） | 基于 **CANN 社区版 9.0.0 / AscendC 9.0.0** 开发。 |
| **目标硬件：Atlas A2** | 2.1 表 2-1（TPosition 映射，约 p.54） | Atlas A2 训练/推理系列：C1→L1 Buffer，C2→BiasTable Buffer，CO2→GM 等；与 GM / UB / L0A/B/C 等存储单元对应。 |
| **NPU / AI Core 规模（项目默认）** | —（项目约定） | Atlas A2 推训服务器常见 **8×NPU**；**每 NPU 默认最多 20 个 AI Core**（可按实机调整）。 |
| **AI Core 组成** | 离线 [基本架构](../offline-web/基本架构-CANN社区版9.0.0-昇腾社区.html)；PDF 1 章 API 列表 + 2.3.2/2.3.3 | 每 AI Core：**1×Cube（矩阵/AIC）+ 2×Vector（向量/AIV）**。官方分离架构下 Cube 与 Vector **无直接数据通路**；融合算子通过 **CrossCore\***、**CubeResGroupHandle**、**Fixpipe** 等机制协作（见下条）。 |
| **Cube–Vector 融合（无显式 GM 往返）** | 1 章 API 列表：CrossCoreSetFlag / CrossCoreWaitFlag（约 p.15–16）；CubeResGroupHandle / GroupBarrier（约 p.16）；2.3.2.1.12 Fixpipe（目录 p.276 起） | 样例（如 leakyrelu 类融合算子）中 Cube 输出可经硬件/运行时路径分发给对应 Vector，代码中往往**不见**「Cube→GM→Vector」显式搬运；更像 Cube0 结果**一分为二**供给 Vector0、Vector1。实现 MIX 算子时需结合 **GetSubBlockNum / GetSubBlockIdx** 区分 AIC/AIV。 |
| **Matmul+LeakyRelu 融合样例（Gitee samples）** | —（代码参考，非 PDF 单节） | `samples/operator/ascendc/tutorials/MatmulLeakyReluCustomSample/KernelLaunch/MatmulLeakyReluInvocation/matmul_leakyrelu_custom.cpp`：`Matmul<…, TPosition::VECIN>` 输出接 Vector 阶段；**工程技术抽象**（剔除激活公式）见 [qa/2026-06-09 纪要 §六](../../qa/2026-06/2026-06-09-AscendC平台与CANN文档索引.md)。 |
| **SIMD vs SIMT** | 目录：第 2 章 vs 第 3 章 | **本项目暂只采用 SIMD**（第 2 章）；SIMT（第 3 章，自 p.2345）后续再考虑。 |
| **SIMD 通用说明和约束** | **2.1**，p.53 起 | 头文件路径（`kernel_operator.h`、`basic_api` / `highlevel_api`）、TPosition↔物理内存表、地址对齐（表 2-2）、地址重叠约束。离线副本：[SIMD API 通用说明和约束](../offline-web/SIMD%20API通用说明和约束-CANN社区版9.0.0-昇腾社区.html)。 |
| **基本架构（昇腾分离架构）** | 离线 [基本架构](../offline-web/基本架构-CANN社区版9.0.0-昇腾社区.html)；PDF 中相关：2.1 存储映射、1 章分离模式 API | 离线页含：关键概念、AI Core 工作模式、计算/存储/搬运单元、典型数据流与指令流。PDF 接口参考以 **API + 约束** 为主，架构叙述以离线页 + 2.1 映射表互补。 |

---

## 维护约定

1. **新增查阅**：遵循上文「查阅工作流」；在「查阅记录」**顶部**表格中增加一行，写明日期、主题、PDF 章/节/页、约束与用法概括（含 A2 数据类型 / CMPMODE 限制）。
2. **页码**：优先写 PDF 印刷页脚页码（与目录一致）；若只有 `pdftotext` 行号，注明「约 p.XX」并在后续核对后修正。
3. **交叉引用**：若有对应离线网页或 `samples/` / `examples/` 代码，在概括列或本段下方用相对链接注明。
4. **子目录 INDEX**：若 PDF 本身增删改版，同步更新 [INDEX.md](INDEX.md) 中该 PDF 一行说明。
