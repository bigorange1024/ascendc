# DRW-S0A · Decrypt 拓扑草图

```text
Host (single ACL session)
│
├─ Launch L1  dec_prep_custom          [AIV-only]  no CrossCore
│     dk_pke,c ──► ŝ, u, v
├─ SynchronizeStream                   ← mid-sync (X14 / I1)
│
├─ Launch L2a dec_ntt_dot_custom       [MIX]  flag∈{1,3,4?}
│     u,ŝ ──► û, ŵ
├─ SynchronizeStream                   ← X15 隔离
│
└─ Launch L2b dec_intt_extract_custom  [MIX]  flag∈{1,3,4?}
      ŵ,v ──► m (32B, DataCopy)

DG1=L1 → DRW-D01 first code
DG2=L2a · DG3=L2b
```
