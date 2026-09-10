# LR-KG-F1 — PKE KeyGen 3→2（融 L2a+L2b）

## 目标

Host：`kg_prep` → sync → **`kg_ntt_dot_encode`（单 MIX）** → sync。  
I/O：`ek_pke`/`dk_pke` ≡ liboqs_pke；NPU×30 不挂。

## 做法

- 自 `RB-K04` 派生 `RB-K07-pke-2launch`。  
- 将 `kg_ntt_custom` 与 `kg_dot_encode_custom` **串行进同一 MIX**；两段 Cube 之间用合法 CrossCore 短握手，**第二段前复位/隔离 flag 生命周期**。  
- basename 新且唯一。

## cannbot

上板前：`sync_audit.py --check all` + flow analyzer → `logs/`。

## runner

`main_npu` only。

## 非目标

不抄 stable KeyGen；不融 prep∥MIX；不跑 CPU/SIM 结案。
