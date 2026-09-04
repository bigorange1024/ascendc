# 2026-09-04 — PKE Encrypt/Decrypt 实机短报（老挂点仍在）

关键字：**PKE** · Encrypt **第7轮** `l18_l19` · Decrypt **FORCE 第1轮** `prod input` · 极短回报

## 用户短报（原意）

| 用例 | 结果 |
|------|------|
| PKE Encrypt | 第 **7** 轮挂；末行仍 `launch 2 f203_encrypt_l18_l19 (compute + inline tail pack -> c)` |
| PKE Decrypt | **FORCE 第1轮**即挂；末行仍 `prod input = dk_pke + c + lut_* … _gen_fixture` |

结论：**老位置未消**；非 KEM 优先线。

## 判读

- Encrypt：粘性型（多轮后挂在 l18）；PKE 仍设备 PrefixEmbed（Hostμ 未迁到 PKE）。
- Decrypt：FORCE 仍挂 → 不是「只吃旧二进制」能解释的全部；挂在 gen_data 打印之后、未见 `1-kernel done` → 指向 fused MIX 未返回。
- 下一刀（Cloud，先 SIM）：Decrypt 加 TRACE 段标记；Encrypt 考虑 **PKE 路径** Host 折 μ / TRACE（勿再默认压 KEM Encaps）。

## 回报约定（强制）

用户手敲：一行式短报即可；禁止再要长日志。
