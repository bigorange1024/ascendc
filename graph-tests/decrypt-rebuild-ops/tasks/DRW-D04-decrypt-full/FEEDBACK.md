ID: PASS_CPU
cmd: cd graph-tests/dec_related/RB-D04-decrypt-full && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: 35
sync_audit: clean（无红线；SYNC-05 薄封装假阳性×2 + SYNC-09 性能提示，同 D02/D03）
notes:
- 新建 `RB-D04-decrypt-full/`：单 binary 三核 basename=`dec_prep_custom` / `dec_ntt_dot_custom` / `dec_intt_extract_custom`（自 D01–D03 复制；头文件 `d02_inc/`/`d03_inc/` 隔离 tiling）。
- Host 三 launch + 每次后 mid-sync（CPU：顺序屏障+日志；SIM/NPU 路径：`aclrtSynchronizeStream`）。
- I/O：`dk_pke[1536]+c[1568]` → `m[32]`；X12；L2a/L2b flag 1/3+4。
- CPU：`m≡oracle` max=0；oracle=**liboqs_pke_ref**（本机有）；TRACE L2a/L2b GATE+SET1+WAIT3 + Cube 非零。
- sync_audit.json → 本刀 `logs/`；未跑 SIM/NPU。
next_hint: 请主控请用户开机后推 `-r npu` + liboqs（关 Q-DEC-CORRECT → 开 K01）。

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：三 launch 全链；`m≡liboqs_pke_ref` max=0（权威 CPU 段已满足）。  

### 主控 NPU 段（同日，云机 `cannlab-npu` / 910B3）

| 项 | 结果 |
|----|------|
| 主机 | `which_npu` → `cannlab-npu` (`100.84.156.61:2222`)；侧树 `/mnt/workspace/ascendc-drw-d04`（远端主仓无本战役未提交码） |
| 编译 | `bash run.sh -r npu -v Ascend910B3` 编通；`ASCEND_DEVICE_ID=0` + `flock .npu.lock` |
| 功能（先） | host_fips oracle：`m` max=0；`wall_sec≈9.98`；PASS_SYNC+PASS_IO |
| 权威 | 远端编 aarch64 `liboqs` 0.15.0 + `liboqs_pke_ref`（勿 rsync x86 二进制） |
| **liboqs NPU** | **`m≡liboqs_pke_ref` max=0**；`wall_sec=2.652`（预算 180）；PASS_SYNC+PASS_IO |
| 结论 | **关 `Q-DEC-CORRECT`**；开 **DRW-K01** |

ID: PASS_NPU
cmd: ASCEND_DEVICE_ID=0 flock … bash run.sh -r npu -v Ascend910B3（侧树；oracle=liboqs_pke_ref）
exit: 0
wall_sec: 2.652

