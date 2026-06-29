#!/usr/bin/env python3
# coding=utf-8
"""P_OUT / S_VEC from env INNERPRODUCT_P_OUT, INNERPRODUCT_S_VEC (default 4)."""
import os

P_OUT = int(os.environ.get("INNERPRODUCT_P_OUT", "4"))
S_VEC = int(os.environ.get("INNERPRODUCT_S_VEC", str(P_OUT)))
N = 256
