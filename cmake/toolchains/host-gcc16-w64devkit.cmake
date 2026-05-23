set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CHARM_HOST_GCC16_ROOT
    "D:/Toolchains/w64devkit"
    CACHE PATH "Root path of the host GCC16 w64devkit toolchain")

set(_CHARM_HOST_GCC16_BIN "${CHARM_HOST_GCC16_ROOT}/bin")

if (NOT EXISTS "${_CHARM_HOST_GCC16_BIN}/g++.exe")
    message(FATAL_ERROR
        "host-gcc16 toolchain requires ${_CHARM_HOST_GCC16_BIN}/g++.exe")
endif()

set(ENV{PATH} "${_CHARM_HOST_GCC16_BIN};$ENV{PATH}")

set(CMAKE_C_COMPILER   "${_CHARM_HOST_GCC16_BIN}/gcc.exe")
set(CMAKE_CXX_COMPILER "${_CHARM_HOST_GCC16_BIN}/g++.exe")
set(CMAKE_RC_COMPILER  "${_CHARM_HOST_GCC16_BIN}/windres.exe")
set(CMAKE_AR           "${_CHARM_HOST_GCC16_BIN}/gcc-ar.exe")
set(CMAKE_RANLIB       "${_CHARM_HOST_GCC16_BIN}/gcc-ranlib.exe")
