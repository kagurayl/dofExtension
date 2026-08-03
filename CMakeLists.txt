cmake_minimum_required(VERSION 3.20)
project(dofExtension LANGUAGES CXX RC)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "dofExtension requires a 32-bit x86 generator (use -A Win32)")
endif()

set(DOFEXT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(DOFEXT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(DOFEXT_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(DOFEXT_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
set(SQUIRREL_211_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/squirrel211")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB GENERATED_CNUT_STREAMS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/closures/*.cnut"
)

set(CLOSURE_PAYLOAD "${DOFEXT_GENERATED_DIR}/all_closures.bin")
set(CLOSURE_MANIFEST "${DOFEXT_GENERATED_DIR}/all_closures_bytecode_manifest.json")
add_custom_command(
    OUTPUT "${CLOSURE_PAYLOAD}" "${CLOSURE_MANIFEST}"
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_closure_bytecode_payload.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/closures/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/closures"
        "${DOFEXT_GENERATED_DIR}/current_chr_dispatch.inc"
        "${CLOSURE_PAYLOAD}"
        "${CLOSURE_MANIFEST}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_closure_bytecode_payload.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/closures/manifest.json"
        "${DOFEXT_GENERATED_DIR}/current_chr_dispatch.inc"
        ${GENERATED_CNUT_STREAMS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(generate_closure_payload
    DEPENDS "${CLOSURE_PAYLOAD}" "${CLOSURE_MANIFEST}"
)

set(DOFEXT_COMMON_SOURCES
    src/common/offset_path.cpp
)
set(DOFEXT_CORE_SOURCES
    src/core/callback_support.cpp
    src/core/callbacks_dispatch.cpp
    src/core/callbacks_ui_network.cpp
    src/core/callbacks_data.cpp
    src/core/callback_registry.cpp
)
set(DOFEXT_HOST_SOURCES
    src/host/absolute_host_api.cpp
    src/host/absolute_memory.cpp
    src/host/absolute_rendering.cpp
    src/host/absolute_services.cpp
    src/host/game_squirrel_vm_adapter.cpp
    src/host/object_hooks.cpp
)
set(DOFEXT_RUNTIME_SOURCES
    src/runtime/dll_entry.cpp
    src/runtime/closure_installer.cpp
)

add_library(dof_extension SHARED
    ${DOFEXT_COMMON_SOURCES}
    ${DOFEXT_CORE_SOURCES}
    ${DOFEXT_HOST_SOURCES}
    ${DOFEXT_RUNTIME_SOURCES}
    resources/generated_closure_payload.rc
    resources/dofExtension.def
)
add_dependencies(dof_extension generate_closure_payload)
set_target_properties(dof_extension PROPERTIES OUTPUT_NAME "dofExtension")
target_include_directories(dof_extension
    PUBLIC "${DOFEXT_INCLUDE_DIR}"
    PRIVATE
        "${DOFEXT_SOURCE_DIR}"
        "${DOFEXT_GENERATED_DIR}"
        "${DOFEXT_RESOURCE_DIR}"
)
target_compile_features(dof_extension PUBLIC cxx_std_17)

file(GLOB SQUIRREL_211_SOURCES CONFIGURE_DEPENDS
    "${SQUIRREL_211_DIR}/squirrel/*.cpp"
)
function(configure_squirrel_test target)
    target_include_directories(${target} PRIVATE
        "${DOFEXT_INCLUDE_DIR}"
        "${DOFEXT_SOURCE_DIR}"
        "${DOFEXT_GENERATED_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        "${SQUIRREL_211_DIR}/include"
        "${SQUIRREL_211_DIR}/squirrel"
    )
    target_compile_definitions(${target} PRIVATE SQUNICODE)
    target_compile_features(${target} PRIVATE cxx_std_17)
endfunction()

add_executable(vm_adapter_selftest EXCLUDE_FROM_ALL
    tests/vm_adapter_selftest.cpp
    tests/support/squirrel211_vm_adapter.cpp
    ${SQUIRREL_211_SOURCES}
)
configure_squirrel_test(vm_adapter_selftest)

add_executable(closure_source_selftest EXCLUDE_FROM_ALL
    tests/closure_source_selftest.cpp
    ${SQUIRREL_211_SOURCES}
)
configure_squirrel_test(closure_source_selftest)

add_executable(closure_bytecode_selftest EXCLUDE_FROM_ALL
    tests/closure_bytecode_selftest.cpp
    ${SQUIRREL_211_SOURCES}
)
configure_squirrel_test(closure_bytecode_selftest)
add_dependencies(closure_bytecode_selftest generate_closure_payload)

add_executable(wrapped_object_selftest EXCLUDE_FROM_ALL
    tests/wrapped_object_selftest.cpp
    src/host/game_squirrel_vm_adapter.cpp
)
target_include_directories(wrapped_object_selftest PRIVATE
    "${DOFEXT_INCLUDE_DIR}"
    "${DOFEXT_SOURCE_DIR}"
)
target_compile_features(wrapped_object_selftest PRIVATE cxx_std_17)

if(MSVC)
    foreach(target IN ITEMS
            dof_extension
            vm_adapter_selftest
            closure_source_selftest
            closure_bytecode_selftest
            wrapped_object_selftest)
        target_compile_options(${target} PRIVATE /W4 /permissive /EHsc /utf-8 /wd4996)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
