cmake_minimum_required(VERSION 3.25)
project(VisionRTContracts VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT TARGET opencv_core OR NOT TARGET opencv_imgcodecs)
    find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)
endif()

set(_visionrt_miniz_gen "${CMAKE_CURRENT_BINARY_DIR}/miniz_gen")
file(MAKE_DIRECTORY "${_visionrt_miniz_gen}")
file(WRITE "${_visionrt_miniz_gen}/miniz_export.h" "#pragma once\n#ifndef MINIZ_EXPORT\n#define MINIZ_EXPORT\n#endif\n")

add_library(VisionRTContracts STATIC
    src/Tool.cpp
    src/Issue.cpp
    src/TypeSpec.cpp
    src/ROI.cpp
    src/GeometryValues.cpp
    src/Affine2D.cpp
    src/Value.cpp
    src/ValueJson.cpp
    src/TrainingSet.cpp
    src/ZipArchive.cpp
    src/TrainingArchive.cpp
    src/Declaration.cpp
    src/JsonDocument.cpp
    src/DeclarationValue.cpp
    src/DeclarationJson.cpp
    3rdparty/miniz/miniz.c
    3rdparty/miniz/miniz_tdef.c
    3rdparty/miniz/miniz_tinfl.c
    3rdparty/miniz/miniz_zip.c)
add_library(VisionRT::Contracts ALIAS VisionRTContracts)
set_target_properties(VisionRTContracts PROPERTIES
    EXPORT_NAME Contracts
    VISIONRT_CONTRACTS_VERSION "${PROJECT_VERSION}"
)
if(NOT PROJECT_IS_TOP_LEVEL)
    set(VISIONRT_CONTRACTS_VERSION "${PROJECT_VERSION}" PARENT_SCOPE)
endif()
target_compile_features(VisionRTContracts PUBLIC cxx_std_20)
target_include_directories(VisionRTContracts PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OpenCV_INCLUDE_DIRS})
# The declaration reader's JSON backend and miniz ZIP backend are private: compiled into the static
# library and never reaching an installed header, so consumers build with the SDK headers alone.
target_include_directories(VisionRTContracts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rapidjson/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/miniz
    ${_visionrt_miniz_gen})
target_compile_definitions(VisionRTContracts PRIVATE MINIZ_NO_STDIO)
target_link_libraries(VisionRTContracts PUBLIC opencv_core PRIVATE opencv_imgcodecs)
if(MSVC)
    target_compile_options(VisionRTContracts PUBLIC /utf-8)
    set_source_files_properties(
        3rdparty/miniz/miniz.c
        3rdparty/miniz/miniz_tdef.c
        3rdparty/miniz/miniz_tinfl.c
        3rdparty/miniz/miniz_zip.c
        PROPERTIES COMPILE_OPTIONS "/W0")
endif()

set(VISIONRT_CONTRACTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE PATH "VisionRT Contracts include directory")
if(NOT PROJECT_IS_TOP_LEVEL)
    set(VISIONRT_CONTRACTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)
endif()

# Install rules are part of the SDK wherever it is built: the host installs one tree and its
# isolated-consumer contract has to configure against it. A consumer may still switch them off.
option(VISIONRT_CONTRACTS_INSTALL "Register install rules for the Contracts SDK" ${PROJECT_IS_TOP_LEVEL})
if(VISIONRT_CONTRACTS_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    install(TARGETS VisionRTContracts EXPORT VisionRTContractsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT VisionRTContractsTargets FILE VisionRTContractsTargets.cmake
        NAMESPACE VisionRT:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VisionRTContracts)
    configure_package_config_file(cmake/VisionRTContractsConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/VisionRTContractsConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VisionRTContracts)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/VisionRTContractsConfigVersion.cmake"
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/VisionRTContractsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/VisionRTContractsConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VisionRTContracts)
endif()

# The contract suite is also registered when Contracts is consumed as a subproject, so the host
# can build and run it as its own `sdk` group instead of pretending an empty filter passed.
option(VISIONRT_CONTRACTS_TESTS "Register the Contracts contract tests" OFF)
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
elseif(VISIONRT_CONTRACTS_TESTS)
    add_subdirectory(tests)
endif()
