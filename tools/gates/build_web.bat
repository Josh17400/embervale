@echo off
rem tools\gates\build_web.bat - S8: the web build exactly as CI does it (emsdk pinned to 6.0.13), incremental.
rem   tools\gates\build_web.bat [builddir]        (default build-web-gates; relative to the repo root)
rem EMSDK must point at an emsdk whose active Emscripten is 6.0.13 (the CI pin, .github/workflows/web.yml).
rem When EMSDK is unset, C:\Users\joshu\emsdk-6.0.13 is used. Exit 0 only when embervale.js and embervale.wasm
rem are built; the runtime export check (web.yml "Check the page's runtime exports") is done by step_gates.sh S8.
setlocal
if "%EMSDK%"=="" set "EMSDK=C:\Users\joshu\emsdk-6.0.13"
if not exist "%EMSDK%\upstream\emscripten\emcc.exe" (
  echo build_web: no emsdk at "%EMSDK%" ^(set EMSDK to an emsdk with 6.0.13 active^)
  exit /b 3
)
set "EM_CONFIG=%EMSDK%\.emscripten"
set "NODEDIR="
for /d %%d in ("%EMSDK%\node\*") do set "NODEDIR=%%d\bin"
set "PYDIR="
for /d %%d in ("%EMSDK%\python\*") do set "PYDIR=%%d"
set "PATH=%EMSDK%\upstream\emscripten;%NODEDIR%;%PYDIR%;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "%~dp0..\.."
set "WB=%~1"
if "%WB%"=="" set "WB=build-web-gates"
call emcc --version 2>nul | findstr /c:"6.0.13" >nul || (
  echo build_web: emcc is not 6.0.13 ^(the CI pin^)
  call emcc --version
  exit /b 4
)
echo build_web: emcc 6.0.13 from %EMSDK%, build dir %WB%
set "SDLSRC=%~dp0..\..\build\_deps\sdl3-src"
if not exist "%WB%\build.ninja" (
  if exist "%SDLSRC%\CMakeLists.txt" (
    call emcmake cmake -S . -B "%WB%" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DFETCHCONTENT_SOURCE_DIR_SDL3=%SDLSRC%" || exit /b 1
  ) else (
    call emcmake cmake -S . -B "%WB%" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
  )
)
cmake --build "%WB%" --target embervale -j8 || exit /b 1
if not exist "%WB%\embervale.js" ( echo build_web: %WB%\embervale.js missing & exit /b 5 )
if not exist "%WB%\embervale.wasm" ( echo build_web: %WB%\embervale.wasm missing & exit /b 5 )
echo build_web: OK %WB%\embervale.js %WB%\embervale.wasm
exit /b 0
