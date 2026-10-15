@echo off
rem Builds the M0 spike XEXs and runs the dialect probe.
rem
rem Run from the "Xbox 360 Command Prompt" (any folder). That prompt sets up
rem the XDK but does not put MSBuild on PATH; this script appends the .NET 4.0
rem MSBuild (the one Visual Studio 2010 uses) to PATH when it is missing. It is
rem appended, not prepended, so "cl" stays the XDK's PowerPC compiler.
rem
rem   xdk\build.cmd            hello, selftest, bench, probe
rem   xdk\build.cmd bigcode    the 64 MB link test (slow)
rem   xdk\build.cmd hello      one project
rem   xdk\build.cmd spire_view the scene viewer (engine + D3D9 renderer)
rem   xdk\build.cmd logic_runner  the game logic's self-play test (M2)
rem   xdk\build.cmd spire360  the game (engine + logic)
rem                            Both link logic_lib (the runtime and the
rem                            translated logic), built first and
rem                            incrementally: an unchanged compile is kept
rem                            (it takes over an hour); set SPIRE_REBUILD=1
rem                            to start over.
rem
rem Logs: xdk\build_logs\<project>.log  (send these if anything fails)
setlocal enabledelayedexpansion
set XDKDIR=%~dp0
set LOGS=%XDKDIR%build_logs
if not exist "%LOGS%" mkdir "%LOGS%"

if not defined XEDK (
  echo XEDK is not set: run this from the "Xbox 360 Command Prompt".
  exit /b 1
)

where msbuild >nul 2>&1
if errorlevel 1 set "PATH=%PATH%;%WINDIR%\Microsoft.NET\Framework\v4.0.30319"
where msbuild >nul 2>&1
if errorlevel 1 (
  echo MSBuild not found. Expected %WINDIR%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe
  exit /b 1
)

rem Files removed from the repository that an older copy updated in place
rem may still hold (the projects compile every .cpp in engine\*).
for %%F in (engine\spine\spine.cpp) do (
  if exist "%XDKDIR%..\%%F" (
    echo Removing %%F ^(from an older version, no longer used^)
    del "%XDKDIR%..\%%F"
  )
)

set PROJECTS=%*
if "%PROJECTS%"=="" set PROJECTS=hello selftest bench probe

set SUMMARY=
for %%P in (%PROJECTS%) do (
  if /i "%%P"=="probe" (
    call "%XDKDIR%probe\probe.cmd" > "%LOGS%\probe.log" 2>&1
    set SUMMARY=!SUMMARY! probe:see-probe_results.txt
  ) else (
    if /i "%%P"=="bigcode" (
      if not exist "%XDKDIR%bigcode\gen\big_table.c" (
        echo Generating bigcode sources ^(64 MB of code^)...
        python "%XDKDIR%bigcode\gen_bigcode.py" --mb 64
      )
    )
    set NEEDS_LIB=
    if /i "%%P"=="logic_runner" set NEEDS_LIB=1
    if /i "%%P"=="spire360" set NEEDS_LIB=1
    if defined NEEDS_LIB if not defined LIB_DONE (
      set LIB_TARGET=Build
      if "%SPIRE_REBUILD%"=="1" set LIB_TARGET=Rebuild
      echo Building logic_lib ^(!LIB_TARGET!; the first time takes over an hour^)...
      msbuild "%XDKDIR%logic_lib\logic_lib.vcxproj" /nologo /t:!LIB_TARGET! /p:Configuration=Release /p:Platform="Xbox 360" /v:minimal /fl "/flp:logfile=%LOGS%\logic_lib.log;verbosity=normal;PerformanceSummary"
      if errorlevel 1 (
        set SUMMARY=!SUMMARY! logic_lib:FAILED
        set LIB_FAILED=1
      ) else (
        set SUMMARY=!SUMMARY! logic_lib:ok
      )
      set LIB_DONE=1
    )
    set SKIP=
    if defined NEEDS_LIB if defined LIB_FAILED set SKIP=1
    if defined SKIP (
      echo Skipping %%P: logic_lib failed ^(see %LOGS%\logic_lib.log^)
      set SUMMARY=!SUMMARY! %%P:skipped
    ) else (
      set TARGET=Rebuild
      echo Building %%P ^(!TARGET!^)...
      msbuild "%XDKDIR%%%P\%%P.vcxproj" /nologo /t:!TARGET! /p:Configuration=Release /p:Platform="Xbox 360" /v:minimal /fl "/flp:logfile=%LOGS%\%%P.log;verbosity=normal;PerformanceSummary"
      if errorlevel 1 (set SUMMARY=!SUMMARY! %%P:FAILED) else (set SUMMARY=!SUMMARY! %%P:ok)
    )
  )
)
echo.
echo Result:%SUMMARY%
echo Logs in %LOGS%
