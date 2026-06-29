cmake_minimum_required(VERSION 3.16)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_STANDARD 17)
project(add_custom_sample)

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
if(NOT DEFINED smoke_testcase)
  message(FATAL_ERROR "Set -Dsmoke_testcase=add_custom")
endif()
if(NOT DEFINED run_mode)
  set(run_mode "npu")
endif()

message(STATUS "CANN device: product_type=${product_type} sim=${sim_device} arch=${ccec_aicore_arch} run_mode=${run_mode}")

# --- NPU / simulator (ccec + device-specific CAModel libs) ---
function(add_ccec_target)
  cmake_parse_arguments(NPU "" "TARGET;OUTPUT_FILE" "CCE_FILE;CPP_FILE" ${ARGN})
  set(ascendc_inc_opts
    "-I${install_path}/include"
    "-I${install_path}/compiler/tikcpp/tikcfw"
    "-I${install_path}/compiler/tikcpp/tikcfw/impl"
    "-I${install_path}/compiler/tikcpp/tikcfw/interface"
    "-I${install_path}/tools/tikicpulib/lib/include"
  )
  if(run_mode STREQUAL "sim")
    set(ccec_runtime_opts
      "-L${install_path}/toolkit/tools/simulator/${sim_device}/lib"
      "-lruntime_camodel" "-lnpu_drv_camodel" "-lpem_davinci"
      "-Wl,-rpath,${install_path}/toolkit/tools/simulator/${sim_device}/lib"
    )
  else()
    set(ccec_runtime_opts
      "-L${install_path}/lib64"
      "-lruntime"
      "-Wl,-rpath,${install_path}/lib64"
    )
  endif()
  set(ccec_base_opts
    "-O2" "-std=c++17"
    ${ccec_runtime_opts}
    "-L${install_path}/lib64"
    "-lascendcl" "-lerror_manager" "-lstdc++"
    "--cce-aicore-arch=${ccec_aicore_arch}"
    "-L${install_path}/tools/tikicpulib/lib/${tikicpu_lib_device}"
  )

  add_custom_target(${NPU_TARGET} ALL
    COMMAND ccec -x cce ${NPU_CCE_FILE} -x c++ ${NPU_CPP_FILE}
      ${ascendc_inc_opts} ${ccec_base_opts}
      "-o" ${NPU_OUTPUT_FILE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building ${NPU_TARGET} (simulator ${sim_device})"
  )
endfunction()

set(cce_src ${CMAKE_SOURCE_DIR}/${smoke_testcase}.cpp)
set(cpp_src ${CMAKE_SOURCE_DIR}/main.cpp)

add_ccec_target(TARGET ${smoke_testcase}_npu
  CCE_FILE ${cce_src}
  CPP_FILE ${cpp_src}
  OUTPUT_FILE ${CMAKE_SOURCE_DIR}/${smoke_testcase}_npu
)

# --- CPU twin-debug (device-specific tikicpulib) ---
if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
  set(CMAKE_PREFIX_PATH ${install_path}/tools/tikicpulib/lib/cmake)
endif()
find_package(tikicpulib REQUIRED)

add_executable(${smoke_testcase}_cpu ${cpp_src} ${cce_src})

target_include_directories(${smoke_testcase}_cpu PRIVATE ${install_path}/include)

target_link_libraries(${smoke_testcase}_cpu PRIVATE tikicpulib::${product_type} ascendcl)

target_compile_options(${smoke_testcase}_cpu PRIVATE -g)

set_target_properties(${smoke_testcase}_cpu PROPERTIES
  OUTPUT_NAME ${smoke_testcase}_cpu
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
