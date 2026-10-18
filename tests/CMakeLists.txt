cmake_minimum_required(VERSION 3.25)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(EXISTS "${OpenCV_DIR}/../bin")
    file(REAL_PATH "${OpenCV_DIR}/../bin" _opencv_bin_dir)
elseif(EXISTS "${OpenCV_DIR}/bin")
    file(REAL_PATH "${OpenCV_DIR}/bin" _opencv_bin_dir)
else()
    set(_opencv_bin_dir "")
endif()

# 1. test_sdk_value_contract
add_executable(test_sdk_value_contract test_sdk_value_contract.cpp)
target_link_libraries(test_sdk_value_contract PRIVATE VisionRT::ToolSDK)
if(MSVC)
    target_compile_options(test_sdk_value_contract PRIVATE /utf-8)
endif()
add_test(NAME test_sdk_value_contract COMMAND test_sdk_value_contract)
if(_opencv_bin_dir)
    set_tests_properties(test_sdk_value_contract PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 2. test_sdk_consumer
add_executable(test_sdk_consumer test_sdk_consumer.cpp)
target_link_libraries(test_sdk_consumer PRIVATE VisionRT::ToolSDK)
if(MSVC)
    target_compile_options(test_sdk_consumer PRIVATE /utf-8)
endif()
add_test(NAME test_sdk_consumer COMMAND test_sdk_consumer)
if(_opencv_bin_dir)
    set_tests_properties(test_sdk_consumer PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 3. test_cli
add_test(
    NAME test_cli
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py"
            --validator "$<TARGET_FILE:visionrt_validate_tool>"
            --opencv-bin "${_opencv_bin_dir}"
)
if(_opencv_bin_dir)
    set_tests_properties(test_cli PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 4. test_isolated_installation
add_test(
    NAME test_isolated_installation
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test_isolated_installation.py"
            --build-dir "${CMAKE_BINARY_DIR}"
            --config "$<CONFIG>"
            --opencv-dir "${OpenCV_DIR}"
            --opencv-bin "${_opencv_bin_dir}"
            --make-program "${CMAKE_MAKE_PROGRAM}"
            --cxx-compiler "${CMAKE_CXX_COMPILER}"
)
if(_opencv_bin_dir)
    set_tests_properties(test_isolated_installation PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()
