#!/usr/bin/python3
# coding=utf-8
# EN01：dst vs golden 对拍（失败由 run.sh 标 CORRECTNESS_SOFT_FAIL，不否决不挂门禁）
# 迁入自 cann-ntt-author-merged_dsa/scripts/verify_result.py

import sys
import numpy as np

# for float32
relative_tol = 0
absolute_tol = 0
error_tol = 0


def verify_result(output, golden):
    output = np.fromfile(output, dtype=np.int32).reshape(-1)
    golden = np.fromfile(golden, dtype=np.int32).reshape(-1)
    different_element_results = np.isclose(output,
                                           golden,
                                           rtol=relative_tol,
                                           atol=absolute_tol,
                                           equal_nan=True)
    different_element_indexes = np.where(different_element_results == False)[0]
    for index in range(len(different_element_indexes)):
        real_index = different_element_indexes[index]
        golden_data = golden[real_index]
        output_data = output[real_index]
        print(
            "data index: %06d, expected: %d(%x), actual: %d(%x), diff: %d(%x)" %
            (real_index, golden_data, golden_data, output_data, output_data, 
             output_data - golden_data, output_data - golden_data))
        if index == 100:
            break
    print("all_bad index: ", different_element_indexes)
    print("output: ", output)
    error_ratio = float(different_element_indexes.size) / golden.size
    print("error ratio: %.4f, tolerance: %.4f" % (error_ratio, error_tol))
    return error_ratio <= error_tol


if __name__ == '__main__':
    try:
        res = verify_result(sys.argv[1], sys.argv[2])
        if not res:
            raise ValueError("[ERROR] result error")
        else:
            print("test pass")
    except Exception as e:
        print(e)
        sys.exit(1)
