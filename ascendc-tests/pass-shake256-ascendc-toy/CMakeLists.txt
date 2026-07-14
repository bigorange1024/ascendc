cmake_minimum_required(VERSION 3.16)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_STANDARD 17)
project(shake256_ascendc_toy)

if(NOT DEFINED install_path)
  set(install_path "$ENV{CANN_HOME}")
endif()
if(NOT install_path)
  message(FATAL_ERROR "Set CANN_HOME or pass -Dinstall_path=...")
endif()

if(NOT DEFINED product_type)
  set(product_type "Ascend910B4")
endif()
if(NOT DEFINED sim_device)
  set(sim_device "Ascend910B4")
endif()
if(NOT DEFINED tikicpu_lib_device)
  set(tikicpu_lib_device "${sim_device}")
endif()
if(NOT DEFINED ccec_aicore_arch)
  set(ccec_aicore_arch "dav-c220")
endif()
if(NOT DEFINED run_mode)
  set(run_mode "cpu")
endif()

set(SMOKE_NAME "shake256_ascendc")
set(REPO_ROOT "${CMAKE_SOURCE_DIR}/../..")
set(KERNEL_ENTRY "${CMAKE_SOURCE_DIR}/shake256_toy_entry.cpp")
set(SHAKE_XOF_INC "${CMAKE_SOURCE_DIR}/../../library/shared/shake_xof_kernel")
set(KECCAK_INC "${CMAKE_SOURCE_DIR}/../../library/shared/keccak_f1600_kernel")

set(SIM_LIB_DIR "${install_path}/tools/simulator/${sim_device}/lib")
if(NOT EXISTS "${SIM_LIB_DIR}")
  set(SIM_LIB_DIR "${install_path}/toolkit/tools/simulator/${sim_device}/lib")
endif()

message(STATUS "pass-shake256-ascendc-toy product=${product_type} run_mode=${run_mode} sim_lib=${SIM_LIB_DIR}")

set(ASCENDC_INC_LIST
  "-I${install_path}/include"
  "-I${install_path}/compiler/tikcpp/tikcfw"
  "-I${install_path}/compiler/tikcpp/tikcfw/impl"
  "-I${install_path}/compiler/tikcpp/tikcfw/interface"
  "-I${install_path}/tools/tikicpulib/lib/include"
  "-I${SHAKE_XOF_INC}"
  "-I${KECCAK_INC}"
  "-I${CMAKE_SOURCE_DIR}/auto_gen"
  "-I${CMAKE_SOURCE_DIR}"
)

if(run_mode STREQUAL "cpu")
  if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${install_path}/tools/tikicpulib/lib/cmake)
  endif()
  find_package(tikicpulib REQUIRED)

  add_library(shake256_kernel_cpu SHARED ${KERNEL_ENTRY})
  target_include_directories(shake256_kernel_cpu PRIVATE ${SHAKE_XOF_INC} ${KECCAK_INC} ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/auto_gen ${install_path}/include)
  target_compile_definitions(shake256_kernel_cpu PRIVATE ASCENDC_CPU_DEBUG)
  target_compile_options(shake256_kernel_cpu PRIVATE -g -O0 -std=c++17)
  target_link_libraries(shake256_kernel_cpu PUBLIC tikicpulib::${product_type})

  add_executable(${SMOKE_NAME}_cpu ${CMAKE_SOURCE_DIR}/main.cpp)
  target_include_directories(${SMOKE_NAME}_cpu PRIVATE ${SHAKE_XOF_INC} ${KECCAK_INC} ${CMAKE_SOURCE_DIR}
    ${install_path}/include)
  target_compile_definitions(${SMOKE_NAME}_cpu PRIVATE ASCENDC_CPU_DEBUG)
  target_compile_options(${SMOKE_NAME}_cpu PRIVATE -g -std=c++17)
  target_link_libraries(${SMOKE_NAME}_cpu PRIVATE shake256_kernel_cpu ascendcl tikicpulib::${product_type})
  set_target_properties(${SMOKE_NAME}_cpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
  if(run_mode STREQUAL "sim")
    set(CCEC_RUNTIME_OPTS
      "-L${SIM_LIB_DIR}"
      "-lruntime_camodel" "-lnpu_drv_camodel" "-lpem_davinci"
      "-Wl,-rpath,${SIM_LIB_DIR}"
    )
  else()
    set(CCEC_RUNTIME_OPTS
      "-L${install_path}/lib64" "-lruntime" "-Wl,-rpath,${install_path}/lib64"
    )
  endif()

  add_custom_target(${SMOKE_NAME}_npu ALL
    COMMAND ccec -x cce ${KERNEL_ENTRY} -x c++ ${CMAKE_SOURCE_DIR}/main.cpp
      ${ASCENDC_INC_LIST}
      "-O2" "-std=c++17" ${CCEC_RUNTIME_OPTS}
      "-L${install_path}/lib64" "-lascendcl" "-lerror_manager" "-lstdc++"
      "--cce-aicore-arch=${ccec_aicore_arch}"
      "-L${install_path}/tools/tikicpulib/lib/${tikicpu_lib_device}"
      "-o" ${CMAKE_SOURCE_DIR}/${SMOKE_NAME}_npu
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building ${SMOKE_NAME}_npu (${run_mode})"
  )
endif()
