@echo off
REM Activates the MSVC x64 build environment, then runs whatever is passed as args.
REM Usage: _env.bat <command> [args...]
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
  echo [Luma] Failed to activate MSVC environment. 1>&2
  exit /b 1
)
%*
