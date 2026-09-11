# LR-DC-F1 — Decrypt 3→2（融 NTT+INTT）

## 目标

Host：`dec_prep` → sync → **单 MIX（原 L2a+L2b）** → m[32]≡liboqs；NPU×30。

## 关键风险

同核 NTT∥INTT **flag 争用**（KB `J-FLAG-CONTENTION`）。必须：**串行两段** + flag 生命周期隔离（或第二段改用仍属 {1,3,4} 且不重叠的调度）。

## cannbot

deadlock-triage 待命；上板前 full sync_audit。

## runner

`main_npu` only。
