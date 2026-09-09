ID: PASS
cmd: ASCEND_DEVICE_ID=0 KERNEL_COMPUTE_BUDGET_SEC=180 bash run.sh -r npu -v Ascend910B3（×30）
exit: 0（×30 全绿）
wall_min: ~13（×30；单轮 kernel≈3.3s + 编译/安装开销）
sync_audit: clean（既有 CPU 刀）
notes:
  - 主机 cannlab-npu；工作树 /mnt/workspace/ascendc-keygen @ dc44067
  - 对拍 liboqs_pke_ref；ek/dk max=0；PASS_SYNC+PASS_IO
  - NPU×30：pass=30 fail=0 hang=0（STOP_ON_FAIL=1）
  - 日志：/mnt/workspace/keygen-npu-logs/K04-x30.{log,exit}
next_hint: 战役已关闸；晋级 stable 须用户 #交付#
