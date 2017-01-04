@REM Copyright (c) Microsoft. All rights reserved.
@REM Licensed under the MIT license. See LICENSE file in the project root for full license information.

@setlocal EnableExtensions EnableDelayedExpansion
@echo off

set current-path=%~dp0

rem // remove trailing slash
set current-path=%current-path:~0,-1%

set build-root=%current-path%\..
rem // resolve to fully qualified path
for %%i in ("%build-root%") do set build-root=%%~fi

set local-install=%build-root%\install-deps

rem ----------------------------------------------------------------------------
rem -- parse script arguments
rem ----------------------------------------------------------------------------

rem // default build options
set build-config=Debug
set build-platform=Win32
set CMAKE_skip_unittests=OFF
set CMAKE_run_e2e_tests=OFF
set build_shared=OFF
set CMAKE_enable_dotnet_binding=OFF
set dotnet_binding_dotnet_core=OFF
set enable_dotnet_binding_pinvoke=OFF
set enable-java-binding=OFF
set enable_nodejs_binding=OFF
set CMAKE_enable_ble_module=ON
set dependency_install_prefix="-Ddependency_install_prefix=%local-install%"

:args-loop
if "%1" equ "" goto args-done
if "%1" equ "--config" goto arg-build-config
if "%1" equ "--platform" goto arg-build-platform
if "%1" equ "--skip-unittests" goto arg-skip-unittests
if "%1" equ "--run-e2e-tests" goto arg-run-e2e-tests
if "%1" equ "--build-shared" goto arg-build-shared
if "%1" equ "--enable-dotnet-binding" goto arg-enable-dotnet-binding
if "%1" equ "--enable-dotnet-core-binding" goto arg-enable-dotnet-core-binding
if "%1" equ "--enable-dotnet-binding-pinvoke" goto arg-enable-dotnet-binding-pinvoke
if "%1" equ "--enable-java-binding" goto arg-enable-java-binding
if "%1" equ "--enable-nodejs-binding" goto arg-enable_nodejs_binding
if "%1" equ "--disable-ble-module" goto arg-disable_ble_module
if "%1" equ "--system-deps-path" goto arg-system-deps-path

call :usage && exit /b 1

:arg-build-config
shift
if "%1" equ "" call :usage && exit /b 1
set build-config=%1
goto args-continue

:arg-build-platform
shift
if "%1" equ "" call :usage && exit /b 1
set build-platform=%1
goto args-continue

:arg-skip-unittests
set CMAKE_skip_unittests=ON
goto args-continue

:arg-run-e2e-tests
set CMAKE_run_e2e_tests=ON
goto args-continue

:arg-build-shared
set build_shared=ON
goto args-continue

:arg-enable-dotnet-binding
if "%CMAKE_enable_dotnet_binding%" equ "ON" call :usage && exit /b 1
set CMAKE_enable_dotnet_binding=ON
set dotnet_binding_dotnet_core=OFF
goto args-continue

:arg-enable-dotnet-core-binding
if "%CMAKE_enable_dotnet_binding%" equ "ON" call :usage && exit /b 1
set CMAKE_enable_dotnet_binding=ON
set dotnet_binding_dotnet_core=ON
goto args-continue

:arg-enable-dotnet-binding-pinvoke
set enable_dotnet_binding_pinvoke=ON
goto args-continue

:arg-enable-java-binding
set enable-java-binding=ON
call %current-path%\build_java.cmd
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!
goto args-continue

:arg-disable_ble_module
set CMAKE_enable_ble_module=OFF
goto args-continue

:arg-enable_nodejs_binding
set enable_nodejs_binding=ON
goto args-continue

:arg-system-deps-path
set dependency_install_prefix=""
goto args-continue

:args-continue
shift
goto args-loop

:args-done

rem -----------------------------------------------------------------------------
rem -- build with CMAKE and run tests
rem -----------------------------------------------------------------------------

rem this is setting the cmake path in a quoted way
set "cmake-root=%build-root%\build"

echo Cleaning up build artifacts...
rmdir /s/q %cmake-root%
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!

git submodule foreach --recursive --quiet "rm -rf build/"
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!

mkdir %cmake-root%
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!

pushd %cmake-root%
if %build-platform% == x64 (
    echo ***Running CMAKE for Win64***
        cmake %dependency_install_prefix% -Dskip_unittests:BOOL=%CMAKE_skip_unittests% -Drun_e2e_tests:BOOL=%CMAKE_run_e2e_tests% -Dbuild_shared:BOOL=%build_shared% -Denable_dotnet_binding:BOOL=%CMAKE_enable_dotnet_binding% -Ddotnet_binding_dotnet_core:BOOL=%dotnet_binding_dotnet_core% -Denable_dotnet_binding_pinvoke:BOOL=%enable_dotnet_binding_pinvoke% -Denable_java_binding:BOOL=%enable-java-binding% -Denable_nodejs_binding:BOOL=%enable_nodejs_binding% -Denable_ble_module:BOOL=%CMAKE_enable_ble_module% "%build-root%" -G "Visual Studio 14 Win64"
        if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!
) else (
    echo ***Running CMAKE for Win32***
        cmake %dependency_install_prefix% -Dskip_unittests:BOOL=%CMAKE_skip_unittests% -Drun_e2e_tests:BOOL=%CMAKE_run_e2e_tests% -Dbuild_shared:BOOL=%build_shared% -Denable_dotnet_binding:BOOL=%CMAKE_enable_dotnet_binding% -Ddotnet_binding_dotnet_core:BOOL=%dotnet_binding_dotnet_core% -Denable_dotnet_binding_pinvoke:BOOL=%enable_dotnet_binding_pinvoke% -Denable_java_binding:BOOL=%enable-java-binding% -Denable_nodejs_binding:BOOL=%enable_nodejs_binding% -Denable_ble_module:BOOL=%CMAKE_enable_ble_module% "%build-root%"
        if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!
)

msbuild /m /p:Configuration="%build-config%" /p:Platform="%build-platform%" azure_iot_gateway_sdk.sln
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!

if "%CMAKE_skip_unittests%"=="ON" if "%CMAKE_run_e2e_tests%"=="OFF" goto skip-tests

ctest -C "debug" -V
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!
:skip-tests

popd
goto :eof

rem -----------------------------------------------------------------------------
rem -- subroutines
rem -----------------------------------------------------------------------------

:usage
echo build.cmd [options]
echo options:
echo  --config value                    Build configuration (e.g. [Debug], Release)
echo  --platform value                  Build platform (e.g. [Win32], x64, ...)
echo  --skip-unittests                  Do not build/run unit tests
echo  --run-e2e-tests                   Build/run end-to-end tests
echo  --build-shared                    Build shared library for dynamic loading
echo  --enable-dotnet-binding           Build the .NET binding
echo                                    (Cannot specify with
echo                                     --enable-dotnet-core-binding)
echo  --enable-dotnet-core-binding      Build the .NET Core binding
echo                                    (Cannot specify with
echo                                     --enable-dotnet-binding)
echo  --enable-dotnet-binding-pinvoke   Enable P/Invoke from .NET/.NET Core for
echo                                    managed gateway mangement API
echo  --enable-java-binding             Build the Java binding
echo                                    (JAVA_HOME must be defined in your
echo                                     environment)
echo  --enable-nodejs-binding           Build Node.js binding
echo                                    (NODE_INCLUDE, NODE_LIB must be defined)
echo  --disable-ble-module              Do not build the BLE module
echo  --system-deps-path                Search for dependencies in a system-level
echo                                    location, e.g. "C:\Program Files (x86)", and
echo                                    install if not found. When this option is 
echo                                    omitted the path is
echo                                    %local-install%.
goto :eof

