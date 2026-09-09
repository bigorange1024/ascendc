ID: PASS
cmd: ASCEND_DEVICE_ID=0 KERNEL_COMPUTE_BUDGET_SEC=180 bash run.sh -r npu -v Ascend910B3（×30）
exit: 0（×30 全绿）
wall_min: ~14（×30；单轮 kernel≈2.5s）
sync_audit: clean（既有 CPU 刀）
notes:
  - 四 launch 全链；oracle=liboqs_kem_ref；ek/dk_kem max=0；PASS_SYNC+PASS_IO
  - 继承 K01 的 kZPrefixBytes 修复后编过
  - NPU×30：pass=30 fail=0 hang=0
  - 日志：/mnt/workspace/keygen-npu-logs/K06-x30.{log,exit}
next_hint: 战役已关闸；晋级 stable 须用户 #交付#
