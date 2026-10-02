@echo off
rem Build TAILSPIN (Release) with MSVC + Ninja.  Usage: tools\build.bat [target]
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "CM=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NJ=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "PATH=%NJ%;%PATH%"
cd /d "%~dp0.."
if not exist build\build.ninja "%CM%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
if "%~1"=="" ("%CM%" --build build) else ("%CM%" --build build --target %~1)
