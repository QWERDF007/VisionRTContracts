@echo off
setlocal

set "PROJECT_ROOT=%~dp0.."
pushd "%PROJECT_ROOT%"

if defined VSCMD_VER goto :VSCMD_INITIALIZED

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio Installer vswhere.exe not found.
    popd
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo [ERROR] Failed to initialize Visual Studio VC environment.
    popd
    exit /b 1
)

:VSCMD_INITIALIZED
setlocal enabledelayedexpansion

set "FIRST_ARG=%~1"

if "%FIRST_ARG%"=="help" goto :SHOW_HELP
if "%FIRST_ARG%"=="-h" goto :SHOW_HELP
if "%FIRST_ARG%"=="--help" goto :SHOW_HELP
if "%FIRST_ARG%"=="/?" goto :SHOW_HELP

if "%FIRST_ARG%"=="clean" goto :DO_BUILD
if "%FIRST_ARG%"=="test" goto :DO_BUILD
if "%FIRST_ARG%"=="" goto :DO_BUILD

set "CHAR1=%FIRST_ARG:~0,1%"
if "%CHAR1%"=="-" goto :DO_BUILD

popd
%*
exit /b %errorlevel%

:DO_BUILD
set "CONFIG=Release"
set "BUILD_DIR=build"
set "DO_CLEAN=0"
set "DO_TEST=0"

:PARSE_ARGS
if "%~1"=="" goto :START_BUILD
if /i "%~1"=="clean" (
    set "DO_CLEAN=1"
    shift
    goto :PARSE_ARGS
)
if /i "%~1"=="--clean" (
    set "DO_CLEAN=1"
    shift
    goto :PARSE_ARGS
)
if /i "%~1"=="test" (
    set "DO_TEST=1"
    shift
    goto :PARSE_ARGS
)
if /i "%~1"=="--test" (
    set "DO_TEST=1"
    shift
    goto :PARSE_ARGS
)
if /i "%~1"=="--config" (
    set "CONFIG=%~2"
    shift
    shift
    goto :PARSE_ARGS
)
if /i "%~1"=="--debug" (
    set "CONFIG=Debug"
    shift
    goto :PARSE_ARGS
)
if /i "%~1"=="--release" (
    set "CONFIG=Release"
    shift
    goto :PARSE_ARGS
)
if /i "%~1"=="--build-dir" (
    set "BUILD_DIR=%~2"
    shift
    shift
    goto :PARSE_ARGS
)
echo [WARN] Unknown option: %~1
shift
goto :PARSE_ARGS

:START_BUILD
where ninja >nul 2>nul
if errorlevel 1 (
    if exist "%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" (
        set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
    ) else (
        echo [WARN] ninja not found on PATH or in Visual Studio; CMake configure may fail if Ninja is requested.
    )
)

if "%DO_CLEAN%"=="1" (
    echo [INFO] Cleaning build directory: %BUILD_DIR%
    if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
)

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [INFO] Configuring VisionRTToolSDK (Ninja, %CONFIG%)
    cmake -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG%
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed.
        popd
        exit /b 1
    )
)

echo [INFO] Building VisionRTToolSDK (%CONFIG%)
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 (
    echo [ERROR] Build failed.
    popd
    exit /b 1
)

if exist "D:\Software\opencv\build\x64\vc16\bin" (
    set "PATH=D:\Software\opencv\build\x64\vc16\bin;%PATH%"
)

if "%DO_TEST%"=="1" (
    echo [INFO] Running CTest
    ctest --test-dir "%BUILD_DIR%" --output-on-failure -C %CONFIG%
    if errorlevel 1 (
        echo [ERROR] Tests failed.
        popd
        exit /b 1
    )
)

popd
exit /b 0

:SHOW_HELP
echo Usage: build.cmd [options]
echo.
echo Options:
echo   clean, --clean         Clean build directory before building
echo   test, --test           Run tests after building
echo   --release              Build in Release configuration (default)
echo   --debug                Build in Debug configuration
echo   --config ^<Config^>      Explicit configuration
echo   --build-dir ^<dir^>      Output directory (default: build)
echo   help, -h, --help       Show this help
echo.
echo Pass arbitrary commands to run in the VS Developer Environment, e.g.:
echo   build.cmd ctest --test-dir build --output-on-failure
popd
exit /b 0
