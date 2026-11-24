include_guard(GLOBAL)

function(visionrt_git_workspace repo git result)
    set(${result} "{\"commit\":\"unknown\",\"branch\":\"unknown\",\"dirty\":\"unknown\"}" PARENT_SCOPE)
    execute_process(COMMAND "${git}" -C "${repo}" rev-parse --show-prefix HEAD
        OUTPUT_VARIABLE _identity RESULT_VARIABLE _code ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE "\r" "" _identity "${_identity}")
    if(NOT _code EQUAL 0 OR NOT _identity MATCHES "^\n([0-9a-f]+)$")
        return()
    endif()
    set(_commit "${CMAKE_MATCH_1}")
    string(LENGTH "${_commit}" _length)
    if(NOT _length EQUAL 40)
        return()
    endif()
    execute_process(COMMAND "${git}" -C "${repo}" rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE _branch RESULT_VARIABLE _code ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _code EQUAL 0 OR _branch STREQUAL "")
        return()
    endif()
    # No filenames are parsed. Disabling rename detection keeps a source-to-document move dirty.
    execute_process(COMMAND "${git}" -C "${repo}" status --porcelain=v1 --no-renames -uall -- .
        ":(exclude,glob)**/WORKLOG.md"
        ":(exclude,glob)**/docs/README.md"
        ":(top,exclude)docs/R[0-9]*.md"
        ":(top,exclude)docs/VisionRT_*.zip"
        OUTPUT_VARIABLE _status RESULT_VARIABLE _code ERROR_QUIET)
    if(NOT _code EQUAL 0)
        return()
    endif()
    if(_status STREQUAL "")
        set(_dirty false)
    else()
        set(_dirty true)
    endif()
    string(REPLACE "\\" "\\\\" _branch "${_branch}")
    string(REPLACE "\"" "\\\"" _branch "${_branch}")
    set(${result} "{\"commit\":\"${_commit}\",\"branch\":\"${_branch}\",\"dirty\":${_dirty}}" PARENT_SCOPE)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    if(NOT DEFINED REPO_DIR OR NOT DEFINED OUTPUT)
        message(FATAL_ERROR "GitWorkspace requires REPO_DIR and OUTPUT")
    endif()
    if(NOT GIT_EXECUTABLE)
        find_program(GIT_EXECUTABLE NAMES git REQUIRED)
    endif()
    visionrt_git_workspace("${REPO_DIR}" "${GIT_EXECUTABLE}" _workspace)
    file(WRITE "${OUTPUT}" "${_workspace}\n")
endif()
