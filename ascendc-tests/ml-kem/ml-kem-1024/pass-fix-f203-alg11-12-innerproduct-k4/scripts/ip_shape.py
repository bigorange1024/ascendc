#!/usr/bin/env python3
# coding=utf-8
"""
@file ip_shape.py
@brief 内积探针形状常量：P_OUT / S_VEC / N，供 gen_data 与 verify_result 共用。

环境变量 INNERPRODUCT_P_OUT、INNERPRODUCT_S_VEC 可覆写（默认 4；S_VEC 默认等于 P_OUT）。
"""
import os

P_OUT = int(os.environ.get("INNERPRODUCT_P_OUT", "4"))
S_VEC = int(os.environ.get("INNERPRODUCT_S_VEC", str(P_OUT)))
N = 256
