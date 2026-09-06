# Encrypt 卡死重写 — 实机一次测清单

> **目标**：一次搬码，跑完对照 + toys + 生产 Encrypt/Encaps。  
> **反馈硬约束**：**不能回传任何文件**；只能在聊天里**打字**报「ID + 状态 + 编号」。  
> **非目标**：本趟不对拍 liboqs。  
> **入口**：[`scripts/npu_hang_rewrite_one_trip.sh`](../../scripts/npu_hang_rewrite_one_trip.sh)

---

## 0. 你只做三件事

1. sync 本仓到实机（见 [`Encrypt卡死重写-sync清单.txt`](./Encrypt卡死重写-sync清单.txt)）  
2. 仓库根跑一条命令（§2）  
3. 把终端末尾 **TYPE_BACK** 整段**打字**贴回聊天（可手改编号）

**不要**回传 `tar` / `md` / 日志 / 截图。脚本本地日志仅供你自己看，Agent 不需要。

---

## 1. 搬码前（无卡也可）

```bash
cd <repo>
unset ASCEND_DEVICE_ID
NPU_HANG_MANIFEST=1 bash scripts/npu_hang_rewrite_one_trip.sh
```

应看到 N0–N10，toys → device **3**，stable → device **1**，并打印 TYPE_BACK 骨架。

---

## 2. 实机：一条命令

```bash
cd <repo>
unset ASCEND_DEVICE_ID
export SOC_VERSION=Ascend910B4
bash scripts/npu_hang_rewrite_one_trip.sh
```

默认：`F203_L18_TRACE=1`、每例强制 rebuild、toys 180s / 生产 600s timeout、失败继续。

跑完看终端 **TYPE_BACK** 块，例如：

```text
N0 通
N1 通 101 201 301 401 402 203 303 199
N8 超时 0 1 2 3
N9 挂 空
```

把这一段打回聊天即可。

---

## 3. 套件（N0–N10）

| ID | 用例 | 卡 |
|----|------|----|
| N0 | PKE KeyGen 对照 | 1 |
| N1–N7 | T01–T07 toys | 3 |
| N8 | PKE Encrypt + L18_TRACE | 1 |
| N9 | KEM Encaps + L18_TRACE | 1 |
| N10 | PKE Decrypt 对照 | 1 |

---

## 4. 打字格式（唯一合法反馈）

每行：`ID 状态 [编号…]`

| 字段 | 取值 |
|------|------|
| 状态 | `通` / `挂` / `超时` / `编译失败` |
| toys 编号 | 屏幕十进制（如 `101 201 … 199`） |
| Encrypt/Encaps | `[l18-trace]` 后的下标（如 `0 1 2 3`）；没有则写 `空` |

---

## 5. Agent 决策树（你可忽略）

| 形态 | 下一刀 |
|------|--------|
| N0 挂 | 环境/卡污染 |
| N0 绿、N1 挂 | MIX NPU 基建 |
| N2 挂、N1 绿 | GATE / Wait(4) |
| toys 全绿、N8 挂 | 生产体量；按 l18 下标开 enc_related |
| N8/N9 同形态 | 并案 Encaps=Encrypt 核 |

**禁止**：继续同质 toys；照抄旧 Encrypt；再要求用户回传文件。
