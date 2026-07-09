# STATUS — pass-fix-f203-alg14-lines3-15-encrypt-prep-k4

**晋级**：2026-07-07 自 `fix-` 重命名为 `pass-`（CPU+SIM 双模式 a_hat/re max=0）。

**状态**：**完成** — Alg.14 行 3–15 设备采样。

**语义**：FIPS 203 **Alg.14 行 3–15 设备采样** — `ek_pke` 尾 `ρ` → `a_hat[16,256]`；`coins` → `r‖e₁‖e₂`（9 poly）；**不含** `t̂` ByteDecode、NTT、行 18+。

**代码来源**：vendoring **仅** [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) `prep/`；**禁止**抄 [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)。

| 阶段 | CPU | SIM | 说明 |
|------|-----|-----|------|
| 方案 | ✅ | — | [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) |
| 单 launch prep | ✅ | ✅ | `f203_encrypt_prep`；SIM tick **470502**（2026-07-06） |

**验收**

```bash
cd ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

**输入**：`fixtures/ek_pke.bin` → `input/ek_pke.bin`（stable `SEED_D=20260619` 固定拷贝）；`coins.bin`（`COINS_SEED` 默认 20260706）。

**自包含**：见 [`SELF_CONTAINED.md`](SELF_CONTAINED.md)。

**实现要点**：PRF host tiling `maxMsgLen=64`（与 `PRF_MSG_STRIDE` 一致，非 33）；PRF 8+1（stable batch8 + nonce8 单条）。
