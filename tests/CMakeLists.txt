find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Threads REQUIRED)

set(_contract_tests "")
foreach(contract_test IN ITEMS test_sdk_value_contract test_data_ref_contract test_array_contract
        test_sdk_function_tool test_sdk_parameters)
    add_executable(${contract_test} ${contract_test}.cpp)
    target_link_libraries(${contract_test} PRIVATE VisionRT::Contracts)
    if(MSVC)
        target_compile_options(${contract_test} PRIVATE /utf-8 /UNDEBUG)
    else()
        target_compile_options(${contract_test} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${contract_test} COMMAND ${contract_test})
    list(APPEND _contract_tests ${contract_test})
endforeach()
target_link_libraries(test_data_ref_contract PRIVATE Threads::Threads)

set(_opencv_bin_dir "")
if(WIN32 AND OpenCV_SHARED)
    set(_opencv_bin_dir "$<TARGET_FILE_DIR:opencv_core>")
    set_tests_properties(test_sdk_value_contract test_data_ref_contract test_array_contract
        test_sdk_function_tool test_sdk_parameters PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

add_test(NAME test_isolated_installation
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_isolated_installation.py"
        --build-dir "${CMAKE_BINARY_DIR}" --config "$<CONFIG>"
        --opencv-dir "${OpenCV_DIR}" --opencv-bin "${_opencv_bin_dir}"
        --make-program "${CMAKE_MAKE_PROGRAM}" --cxx-compiler "${CMAKE_CXX_COMPILER}"
        --generator "${CMAKE_GENERATOR}" --generator-platform "${CMAKE_GENERATOR_PLATFORM}")

list(APPEND _contract_tests test_isolated_installation)

# The suite names its own group and publishes the member list, so a consumer can build it as one
# group (`tests-sdk` in VisionRT) and refuse the empty filter instead of reporting a false green.
foreach(_contract_test IN LISTS _contract_tests)
    set_tests_properties(${_contract_test} PROPERTIES LABELS "sdk")
    set_property(GLOBAL APPEND PROPERTY VISIONRT_CONTRACTS_REGISTERED_TESTS ${_contract_test})
endforeach()
