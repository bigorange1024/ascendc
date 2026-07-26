# chain_sim_staged.cmake — SIM 分阶段：Launch1 用 f203_se_vector_npu，Launch2 用 vec-k4 bbit
set(VEC_K4_BUILD "${CMAKE_BINARY_DIR}/vec_k4_ntt")

add_custom_command(
  OUTPUT ${VEC_K4_BUILD}/.stamp
  COMMAND ${CMAKE_COMMAND} -E make_directory ${VEC_K4_BUILD}
  COMMAND ${CMAKE_COMMAND} -S ${VEC_K4_ROOT} -B ${VEC_K4_BUILD}
    -DRUN_MODE=sim
    -DSOC_VERSION=${product_type}
    -DASCEND_CANN_PACKAGE_PATH=${install_path}
    -DHAT_BYTE_ENCODE=0
    -DHAT_LINE18_DOT_ONLY=1
    -DHAT_ALG11_VEC=1
    -DBYTE_ENCODE12_VEC=1
    -DBYTE_ENCODE12_SCATTER_VEC=1
    -DBYTE_ENCODE12_PREFETCH=0
    -DALG11_IMPL=1
    -DALG11_VEC_VARIANT=2
    -DALG11_VEC_OPTS=1
    -DALG11_MEM_OPS=1
    -DF203_STAGE1_SPLIT=1
  COMMAND ${CMAKE_COMMAND} --build ${VEC_K4_BUILD} -j
  COMMAND ${CMAKE_COMMAND} -E touch ${VEC_K4_BUILD}/.stamp
  COMMENT "Building vec-k4-v2 ascendc_kernels_sim (mixPass=5 Launch2)"
)

add_custom_target(vec_k4_ntt_kernels_sim ALL DEPENDS ${VEC_K4_BUILD}/.stamp)
