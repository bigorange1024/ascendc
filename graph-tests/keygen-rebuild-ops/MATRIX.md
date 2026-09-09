# KeyGen NPU 加压矩阵

> 刷新：2026-09-09（CPU 齐；NPU 待 Cloud 上板后回写）

| 刀 | 目录 | 测什么 | 对拍 | CPU | NPU | ×30 |
|----|------|--------|------|-----|-----|-----|
| P01 | RB-K01-kg-prep | prep Â,ŝ,ê | host/FIPS | PASS | — | — |
| P02 | RB-K02-kg-ntt | NTT(ŝ/ê) | ntt oracle | PASS | — | — |
| P03 | RB-K03-kg-dot-encode | t̂+ek/dk | host FIPS | PASS | — | — |
| P04 | RB-K04-pke-full | PKE 全链 | liboqs_pke | PASS | wait_npu | — |
| K01 | RB-K05-kem-tail | KEM tail | host 契约 | PASS | wait_npu | — |
| K02 | RB-K06-kem-full | KEM 全链 | liboqs_kem | PASS | wait_npu | — |
