# AGENT_HANDOFF

**日期**：2026-09-06  
**分支**：`cursor/kem-2launch-sticky-1534`

## 当前真相

- Encrypt 实机粘性挂根因 **未钉死**；SIM **不能**复现粘性挂。
- 用户锁：ByteDecode / 正确性比对在卡死点之后，**找出挂因前不做**。
- E01–E16 SIM 重写积木 + `graph_tests/npu_suite/`（C0/C1/C2）已齐；SIM smoke 绿。
- **下一步**：用户按 `graph_tests/npu_suite/TOMORROW_NPU.md` 上机，填 `REPORT_TEMPLATE.md`；主控按 `BRANCHING.md` 选下一刀。

## 上机入口

```bash
cd graph_tests/npu_suite
bash run_all_npu.sh -v Ascend910B4
```

## 知识库 / 图谱

- `docs/notes/Encrypt-实机无卡死-知识库.md`（含 §2.1 KeyGen 不挂、§2.2 编译不对称）
- `docs/rg-encrypt-npu-hangfree.yaml` + `.html`

## 禁止

- 零散请测旧 Encrypt；未统一 FORCE/SKIP 的 KeyGen vs Encaps「稳」对照当根因
- 复测 retracted；开正确性/ByteDecode 刀
