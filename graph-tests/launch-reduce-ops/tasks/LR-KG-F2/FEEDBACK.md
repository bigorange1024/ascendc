# LR-KG-F2 FEEDBACK — KEM KeyGen 3→2（RB-K09）

## 结果
**PASS_NPU×30**（2026-09-10）

| 项 | 值 |
|----|-----|
| 目录 | `graph-tests/kg_related/RB-K09-kem-2launch` |
| Host launch | **2**（prep → 融合 MIX 内嵌 kem_tail） |
| Launch3 | **0**（日志无 Launch3） |
| ×30 | ok=30 fail=0 |
| 对拍 | ek/dk_kem max_abs=0 vs liboqs_kem_ref |
| 日志 | `/mnt/workspace/launch-reduce-logs/k09-x30-20260910-114035.log` |
| 单轮冒烟 | `/mnt/workspace/launch-reduce-logs/k09-smoke-20260910-113856.log` |

## 变更
- `kem_tail_device::KemTailRun` 内嵌融合 MIX 段2 AIV0 收尾
- Host 去掉独立 `kg_kem_tail_custom` launch；CMake KERNEL_FILES 仅 prep + fused MIX

## sync_audit
- SYNC-05 命中与 K08 同形 CrossWait 模板 / 非 KERNEL stub；无新增 SoftSync / 非法 flag
- 以 NPU×30 为结案依据

## 备注
- verify 成功横幅文案仍残留 “RB-K08/RB-K07” 字符串（cosmetic）；实际为 2-launch + IO 绿
