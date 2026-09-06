# Encrypt 卡死重写 — 实机一次测清单

> **目标**：一次搬码，跑完对照 + toys + 生产 Encrypt/Encaps，带回 TRACE 反馈。  
> **非目标**：本趟不要求对拍 liboqs；挂死证据优先。  
> **配套**：[`scripts/npu_hang_rewrite_one_trip.sh`](../../scripts/npu_hang_rewrite_one_trip.sh) · KB [`docs/notes/Encrypt-hang-rewrite-kb.md`](../notes/Encrypt-hang-rewrite-kb.md)

---

## 0. 你只做三件事

1. **sync 本仓相关路径到实机**（见 [`Encrypt卡死重写-sync清单.txt`](./Encrypt卡死重写-sync清单.txt)）  
2. **在仓库根执行一条命令**（见 §2）  
3. **把 `FEEDBACK.md` 填上编号 + `BRING_BACK.tar.gz` 回传**

不要逐个用例手跑；脚本已 timeout 隔离，单例挂了会继续下一项。

---

## 1. 搬码前（无卡也可）

```bash
cd <repo>
unset ASCEND_DEVICE_ID
NPU_HANG_MANIFEST=1 bash scripts/npu_hang_rewrite_one_trip.sh
```

应看到 N0–N10 清单，toys → device **3**，stable → device **1**。

分卡自检：

```bash
bash scripts/npu_device_map.sh --self-test
```

---

## 2. 实机：一条命令

```bash
cd <repo>
unset ASCEND_DEVICE_ID
export SOC_VERSION=Ascend910B4
# 可选：预热 CANN / 确认卡干净后再跑
bash scripts/npu_hang_rewrite_one_trip.sh
```

默认已开：

| 项 | 值 |
|----|-----|
| `F203_L18_TRACE` | `1`（Encrypt/Encaps fused-trace 轮询） |
| `--force-rebuild` | 每例强制（避免旧 fused 二进制） |
| toys 超时 | 180s |
| 生产超时 | 600s |
| 失败继续 | `NPU_HANG_CONTINUE=1` |

产物：`output/npu_hang_rewrite/<stamp>/BRING_BACK.tar.gz` + `FEEDBACK.md`。

### 变体（一般不用）

```bash
NPU_HANG_SKIP_TOYS=1 bash scripts/npu_hang_rewrite_one_trip.sh   # 只要 stable
NPU_HANG_SKIP_PROD=1 bash scripts/npu_hang_rewrite_one_trip.sh   # 只要 toys+KeyGen
```

---

## 3. 套件内容（N0–N10）

| ID | 用例 | 卡 | 看什么 |
|----|------|----|--------|
| N0 | stable PKE KeyGen | 1 | 对照：MIX 基线是否健康 |
| N1 | T01 最短 MIX 1/3 | 3 | 最短握手 TRACE |
| N2 | T02 生产 GATE 时序 | 3 | Wait(4) 生产时序 |
| N3 | T03 全 FSM | 3 | NTT→GATE→INTT |
| N4 | T04 假循环×10 | 3 | 体量（SIM 已证非根因） |
| N5 | T05 2×launch | 3 | 多 launch |
| N6 | T06 真 Vec MAC | 3 | GATE 真积木 |
| N7 | T07 SAMPLE→FSM | 3 | 采样前置 |
| N8 | stable PKE Encrypt + L18_TRACE | 1 | **主嫌疑** |
| N9 | stable KEM Encaps + L18_TRACE | 1 | 同核路径并案 |
| N10 | stable PKE Decrypt | 1 | 可选对照 |

---

## 4. 反馈怎么填

打开 `FEEDBACK.md`（或 `BRING_BACK/logs/FEEDBACK.md`）：

- 每行填 **通 / 挂 / 超时 / 编译失败**
- TRACE：toys 抄十进制编号；Encrypt/Encaps 抄 `[l18-trace] …` 下标

Agent 用决策树（你可忽略）见反馈单末尾。

---

## 5. 决策树（Agent 回传后）

| 形态 | 下一刀 |
|------|--------|
| N0 挂 | 环境/卡污染；先清卡再谈 Encrypt |
| N0 绿、N1 挂 | MIX NPU 基建；勿开生产体量 |
| N2 挂、N1 绿 | GATE / Wait(4) 线 |
| toys 全绿、N8 挂 | **生产体量/编排**；按 l18-trace 下标开 `graph-tests/enc_related/` |
| N8/N9 同形态 | 并案 Encaps=Encrypt 核 |
| N8 绿 | 卡死可能已随 FORCE_REBUILD/环境消失；仍记 KB，勿宣称根因已解 |

**禁止**：回传后继续同质 toys；禁止照抄旧 Encrypt。

---

## 6. 与旧 `npu_kem_one_trip` 的关系

| | 本套件 | `npu_kem_one_trip` |
|--|--------|-------------------|
| 目的 | Encrypt **卡死**定位 | KEM 教材/msprof 验收 |
| TRACE | toys 编号 + L18 fused | Encaps L18 一段 |
| 范围 | KeyGen + T01–T07 + Encrypt/Encaps/Decrypt | 更广（roundtrip/教材档） |

卡死未定位前，**优先本套件**；不要混跑拖长借入时间。
