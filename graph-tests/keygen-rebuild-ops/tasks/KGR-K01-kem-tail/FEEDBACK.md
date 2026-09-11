ID: PASS
cmd: ASCEND_DEVICE_ID=0 KERNEL_COMPUTE_BUDGET_SEC=180 bash run.sh -r npu -v Ascend910B3（×30）
exit: 0（×30 全绿）
wall_min: ~8（×30；单轮 kernel≈3.3s）
sync_audit: clean（既有 CPU 刀）
notes:
  - 首轮 NPU 编不过：device 侧 `const char* = "..."` → `__gm__ char[]` 不可赋（CPU 孪生曾假过）
  - 修复：`constexpr uint8_t kZPrefixBytes[]`；K05/K06 同改；CPU+SIM 复验绿
  - NPU×30：pass=30 fail=0 hang=0；H/z/dk_kem max=0
  - 日志：/mnt/workspace/keygen-npu-logs/K05-x30.{log,exit}
next_hint: 战役已关闸
