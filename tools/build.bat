@echo off
rem Build (Release) with MSVC + Ninja.  Usage: tools\build.bat [target]
rem Set BDIR to use a separate build directory (e.g. set BDIR=build_art) so parallel builds don't collide.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "CM=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NJ=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "PATH=%NJ%;%PATH%"
cd /d "%~dp0.."
if "%BDIR%"=="" set "BDIR=build"
set "SDLSRC=%~dp0..\build\_deps\sdl3-src"
if not exist %BDIR%\build.ninja (
  if exist "%SDLSRC%\CMakeLists.txt" (
    "%CM%" -S . -B %BDIR% -G Ninja -DCMAKE_BUILD_TYPE=Release "-DFETCHCONTENT_SOURCE_DIR_SDL3=%SDLSRC%" || exit /b 1
  ) else (
    "%CM%" -S . -B %BDIR% -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
  )
)
if "%~1"=="" ("%CM%" --build %BDIR%) else ("%CM%" --build %BDIR% --target %~1)
