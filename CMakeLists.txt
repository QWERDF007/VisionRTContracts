cmake_minimum_required(VERSION 3.25)
project(VisionRTToolSDK VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET opencv_core)
    if(NOT DEFINED OpenCV_DIR AND NOT DEFINED ENV{OpenCV_DIR})
        set(_opencv_root "")
        if(DEFINED OpenCV_HOME)
            set(_opencv_root "${OpenCV_HOME}")
        elseif(DEFINED ENV{OpenCV_HOME})
            set(_opencv_root "$ENV{OpenCV_HOME}")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies.yaml")
            file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies.yaml" _dep_lines)
            foreach(_line IN LISTS _dep_lines)
                if(_line MATCHES "^[ ]*default:[ ]*(.+)$")
                    string(REGEX REPLACE "^[ ]*default:[ ]*" "" _def_path "${_line}")
                    string(REGEX REPLACE "^[\"'](.*)[\"']$" "\\1" _def_path "${_def_path}")
                    string(STRIP "${_def_path}" _def_path)
                    if(EXISTS "${_def_path}")
                        set(_opencv_root "${_def_path}")
                        break()
                    endif()
                endif()
            endforeach()
        endif()

        if(_opencv_root)
            foreach(_cand IN ITEMS "${_opencv_root}/lib" "${_opencv_root}/lib/cmake/opencv4" "${_opencv_root}")
                if(EXISTS "${_cand}/OpenCVConfig.cmake")
                    set(OpenCV_DIR "${_cand}" CACHE PATH "OpenCV CMake package directory" FORCE)
                    break()
                endif()
            endforeach()
        endif()
    endif()
    find_package(OpenCV REQUIRED COMPONENTS core)
endif()

if(NOT TARGET yaml-cpp::yaml-cpp)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp/CMakeLists.txt")
        set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests" FORCE)
        set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools" FORCE)
        set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib" FORCE)
        set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build static yaml-cpp" FORCE)
        set(YAML_MSVC_SHARED_RT ON CACHE BOOL "MSVC shared runtime" FORCE)
        set(YAML_CPP_INSTALL ON CACHE BOOL "Enable yaml-cpp install" FORCE)
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp" "${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp")
    else()
        message(FATAL_ERROR "yaml-cpp target not found and 3rdparty/yaml-cpp submodule missing. Run: git submodule update --init --recursive")
    endif()
endif()

add_library(VisionRTToolSDK STATIC
    src/Contract.cpp
    src/Package.cpp
    src/Schema.cpp
    src/SchemaYaml.cpp
    src/Value.cpp
    src/ValueYaml.cpp
)
add_library(VisionRT::ToolSDK ALIAS VisionRTToolSDK)
set_target_properties(VisionRTToolSDK PROPERTIES EXPORT_NAME ToolSDK)

target_compile_features(VisionRTToolSDK PUBLIC cxx_std_20)
target_compile_definitions(VisionRTToolSDK PUBLIC YAML_CPP_STATIC_DEFINE)

target_include_directories(VisionRTToolSDK
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(VisionRTToolSDK PUBLIC opencv_core yaml-cpp::yaml-cpp)

if(MSVC)
    target_compile_options(VisionRTToolSDK PUBLIC /utf-8)
endif()

add_executable(visionrt_validate_tool tools/visionrt_validate_tool.cpp)
add_executable(VisionRT::visionrt_validate_tool ALIAS visionrt_validate_tool)
set_target_properties(visionrt_validate_tool PROPERTIES EXPORT_NAME visionrt_validate_tool)
target_link_libraries(visionrt_validate_tool PRIVATE VisionRTToolSDK)

if(MSVC)
    target_compile_options(visionrt_validate_tool PRIVATE /utf-8)
endif()

option(VISIONRT_BUILD_PYTHON_INTEROP "Build optional VisionRT::PythonInterop library" OFF)
if(VISIONRT_BUILD_PYTHON_INTEROP)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Embed)
    if(NOT TARGET pybind11::embed)
        set(PYBIND11_FINDPYTHON ON)
        find_package(pybind11 CONFIG REQUIRED)
    endif()
    add_library(VisionRTPythonInterop STATIC
        src/PythonInterop.cpp
    )
    add_library(VisionRT::PythonInterop ALIAS VisionRTPythonInterop)
    set_target_properties(VisionRTPythonInterop PROPERTIES EXPORT_NAME PythonInterop)

    target_compile_features(VisionRTPythonInterop PUBLIC cxx_std_20)
    target_include_directories(VisionRTPythonInterop
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(VisionRTPythonInterop PUBLIC VisionRTToolSDK pybind11::embed)
    target_compile_definitions(VisionRTPythonInterop PUBLIC PYBIND11_DETAILED_ERROR_MESSAGES)
    if(BUILD_TESTING OR VISIONRT_ENABLE_TEST_PROBES)
        target_compile_definitions(VisionRTPythonInterop PUBLIC VISIONRT_TEST_PROBES)
    endif()
    if(MSVC)
        target_compile_options(VisionRTPythonInterop PUBLIC /utf-8)
    endif()
endif()

set(VISIONRT_TOOL_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE PATH "VisionRT Tool SDK include directory")
if(NOT PROJECT_IS_TOP_LEVEL)
    set(VISIONRT_TOOL_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(_sdk_install_targets VisionRTToolSDK visionrt_validate_tool)
    if(TARGET VisionRTPythonInterop)
        list(APPEND _sdk_install_targets VisionRTPythonInterop)
    endif()

    install(TARGETS ${_sdk_install_targets}
        EXPORT VisionRTToolSDKTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT VisionRTToolSDKTargets
        FILE VisionRTToolSDKTargets.cmake
        NAMESPACE VisionRT::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VisionRTToolSDK
    )

    configure_package_config_file(
        cmake/VisionRTToolSDKConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/VisionRTToolSDKConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VisionRTToolSDK
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/VisionRTToolSDKConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/VisionRTToolSDKConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/VisionRTToolSDKConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VisionRTToolSDK
    )

    include(CTest)
    if(BUILD_TESTING)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
