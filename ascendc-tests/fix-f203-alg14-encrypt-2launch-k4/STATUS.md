# STATUS — fix-f203-alg14-encrypt-2launch-k4

**定位**：FIPS 203 Alg.14（ML-KEM-1024 PKE.Encrypt，k=4）设备全链正确性探针，
**按 keygen 蓝本从零重建**（单 ACL session）。见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)、[`SELF_CONTAINED.md`](SELF_CONTAINED.md)。

**阶段**：**全链打通**（2026-06-30）。CPU + SIM 单 session 全链 `c.bin` 1568B **max=0**。

## 验收证据（默认命令，无手动 export）

```bash
# CPU 全链
ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
#   → G1/G2/G3 verify_gate max=0；[verify] PASS max=0 (1568 bytes)；[SUCCESS] (cpu)

# SIM 全链（单 session，~13 min）
ENCRYPT_KERNEL_BUDGET_SEC=1000 ENCRYPT_VERIFY=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   → G1/G2/G3 verify_gate max=0；[verify] PASS max=0 (1568 bytes)；[SUCCESS] (sim)
#   → 用例根无 stray dump（无 *.dump/core*/profile_*_log*.toml）
```

> SIM 全链计算较慢（G4/G5 tail），默认 `ENCRYPT_KERNEL_BUDGET_SEC` 15s 不够，验收用 1000s。

## Stage Gate 进度

| Gate | 内容 | 设备 | golden | 状态 |
|------|------|------|--------|------|
| 骨架 | 目录 + 蓝图文档 | — | — | ✅ |
| G1 | prep：ρ→â、coins→r,e₁,e₂、ek→t̂（设备 ByteDecode） | ✅ | ✅ | CPU+SIM ✅ |
| G2 | NTT(r)→r̂ | ✅ | ✅ | CPU+SIM ✅ |
| G3 | û=Âᵀ·r̂、tr̂=t̂·r̂（合并核 at_r5 一次 launch） | ✅ | ✅ | CPU+SIM ✅ |
| G4 | INTT + e₁/e₂ + Decompress₁(m)→u,v | ✅ | ✅ | CPU+SIM ✅ |
| G5 | Compress₁₁/₅ + ByteEncode→c | ✅ | ✅ | CPU+SIM ✅ |
| 全链 | CPU+SIM c.bin 1568B max=0 | ✅ | ✅ | **CPU+SIM ✅** |

## 关键病根与解法（SIM 单 session）

### 1) AIV kernel **func_key ≥ 5 → 507000**（CAModel 单 binary 门禁）

- 受控实验：proven `at_r`(key4) PASS、`at_r5`(key5) FAIL、`at_r5` 退化 `kP5=4` 仍 FAIL、`nm` 确认符号在；
  历史 `g3_linear`(key6)/`g3_linear4`(key7) 亦 507000。边界恰在 4↔5。KeyGen 能跑正因其恰好 5 个 AIV 核(key0-4)。
- **解法**：SIM 设备侧 `g3_linear.cpp` **只编 `at_r5`**；`g3_linear/g3_linear4/at_r/t_dot_r` 用
  `#ifdef ASCENDC_CPU_DEBUG` 仅留 CPU（CPU 独立 binary、func_key 无意义）→ SIM AIV 核 =
  `marker/prep_a_hat/prep_re/g4_noise/at_r5` 共 5 个，`at_r5` 落 key4。`main_encrypt.cpp` 旧 staged
  gate<5 SIM G3（独立 session at_r×N）已删除；旧 `<<<>>>` `*_do` 壳删除。

### 2) host 拼 `matM` 前缺 `aclrtSynchronizeStream` → û 全 0

- `at_r5` 用 host 读回 `aHatDev`/`tHatDev` 拼 5×4 矩阵 `matM`(列步长 5：p<4=Â[j,p]、p=4=t̂[j])。
  D2H 前未同步 → prep_a_hat/decode_t_hat 异步未完成 → matM 的 Â 列取到 0 → û=Σ0·r̂=0
  （proven `at_r` 直接在设备读 aHatDev 故无此问题，曾误导为「2nd AIV 不可靠」）。
- **解法**：matM 打包 D2H 前 `aclrtSynchronizeStream(stream)`。

## 关键铁律（详 INTEGRATION_PLAN §3）

1. 单 ACL session（一次 aclInit/aclFinalize）。
2. prep = `AIV_ONLY` 最先 launch；NTT/INTT/pack/g4_noise_ws = MIX 单文件单 kernel（避 auto_gen 降级）。
3. **SIM AIV 核总数 ≤ 5**（func_key 门禁）；新增 SIM AIV 核须复核 `nm device_aiv.o` 的 key≤4。
4. host↔device 往返打包前必同步 stream。
5. CPU `#ifdef ASCENDC_CPU_DEBUG` 走 AIV_ONLY g3_linear4；SIM/NPU 走 at_r5（Twin Path 共用 golden）。

## 资产

- Python golden（黑盒 oracle）：`scripts/host_golden/`（FIPS 203 NTT/INTT/basemul/Compress/ByteEncode/embed）+ `gate_g1/g2/g3.py`、`golden_c.py`。
- 计算核：从旧 encrypt 探针（CPU PASS）+ keygen vendored 复制后重组 launch 编排。
