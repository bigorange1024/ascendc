#!/usr/bin/env python3
"""mlkem_param.py — ML-KEM 参数组长度与 liboqs 算法名（KEM 交叉/RT 共用）。

用法：
  from mlkem_param import resolve_param, PARAMS
  p = resolve_param(os.environ.get("MLKEM_PARAM", "1024"))
"""
from __future__ import annotations

from typing import Any

# FIPS 203 Table 2 / liboqs kem_ml_kem.h
PARAMS: dict[str, dict[str, Any]] = {
    "512": {
        "k": 2,
        "ek": 800,
        "dk": 1632,
        "ct": 768,
        "ss": 32,
        "kem_seed": 64,
        "encaps_seed": 32,
        "oqs_alg": "ML-KEM-512",
        "tag": "512",
    },
    "768": {
        "k": 3,
        "ek": 1184,
        "dk": 2400,
        "ct": 1088,
        "ss": 32,
        "kem_seed": 64,
        "encaps_seed": 32,
        "oqs_alg": "ML-KEM-768",
        "tag": "768",
    },
    "1024": {
        "k": 4,
        "ek": 1568,
        "dk": 3168,
        "ct": 1568,
        "ss": 32,
        "kem_seed": 64,
        "encaps_seed": 32,
        "oqs_alg": "ML-KEM-1024",
        "tag": "1024",
    },
}

_ALIASES = {
    "512": "512",
    "768": "768",
    "1024": "1024",
    "ml-kem-512": "512",
    "ml-kem-768": "768",
    "ml-kem-1024": "1024",
    "ml_kem_512": "512",
    "ml_kem_768": "768",
    "ml_kem_1024": "1024",
    "ML-KEM-512": "512",
    "ML-KEM-768": "768",
    "ML-KEM-1024": "1024",
}


def normalize_param(raw: str | None) -> str:
    """把用户输入规范成 '512'|'768'|'1024'；非法则抛 ValueError。"""
    if raw is None or str(raw).strip() == "":
        return "1024"
    key = str(raw).strip()
    tag = _ALIASES.get(key)
    if tag is None:
        raise ValueError(f"unknown MLKEM_PARAM={raw!r}; want 512|768|1024")
    return tag


def resolve_param(raw: str | None = None) -> dict[str, Any]:
    """返回 PARAMS 条目（含 tag/k/ek/dk/ct/...）。默认 1024。"""
    return PARAMS[normalize_param(raw)]
