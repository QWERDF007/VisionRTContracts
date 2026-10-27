cmake_minimum_required(VERSION 3.25)
project(VisionRTContracts VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT TARGET opencv_core)
    find_package(OpenCV REQUIRED COMPONENTS core)
endif()

add_library(VisionRTContracts STATIC src/Tool.cpp src/TypeSpec.cpp src/ROI.cpp src/Value.cpp)
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
target_link_libraries(VisionRTContracts PUBLIC opencv_core)
if(MSVC)
    target_compile_options(VisionRTContracts PUBLIC /utf-8)
endif()

set(VISIONRT_CONTRACTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE PATH "VisionRT Contracts include directory")
if(NOT PROJECT_IS_TOP_LEVEL)
    set(VISIONRT_CONTRACTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)
endif()

if(PROJECT_IS_TOP_LEVEL)
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
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
