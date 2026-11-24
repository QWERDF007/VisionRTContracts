include_guard(GLOBAL)

function(visionrt_add_enum_exporter target source)
    add_executable(${target} ${source} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EnumExport.cpp")
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /Zc:preprocessor)
    endif()
endfunction()
