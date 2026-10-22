find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(test_sdk_value_contract test_sdk_value_contract.cpp)
target_link_libraries(test_sdk_value_contract PRIVATE VisionRT::Contracts)
if(MSVC)
    target_compile_options(test_sdk_value_contract PRIVATE /utf-8 /UNDEBUG)
else()
    target_compile_options(test_sdk_value_contract PRIVATE -UNDEBUG)
endif()
add_test(NAME test_sdk_value_contract COMMAND test_sdk_value_contract)

set(_opencv_bin_dir "")
if(WIN32 AND OpenCV_SHARED)
    set(_opencv_bin_dir "$<TARGET_FILE_DIR:opencv_core>")
    set_tests_properties(test_sdk_value_contract PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

add_test(NAME test_isolated_installation
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_isolated_installation.py"
        --build-dir "${CMAKE_BINARY_DIR}" --config "$<CONFIG>"
        --opencv-dir "${OpenCV_DIR}" --opencv-bin "${_opencv_bin_dir}"
        --make-program "${CMAKE_MAKE_PROGRAM}" --cxx-compiler "${CMAKE_CXX_COMPILER}")
