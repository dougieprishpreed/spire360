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
    echo Building %%P...
    msbuild "%XDKDIR%%%P\%%P.vcxproj" /nologo /t:Rebuild /p:Configuration=Release /p:Platform="Xbox 360" /v:minimal /fl "/flp:logfile=%LOGS%\%%P.log;verbosity=normal;PerformanceSummary"
    if errorlevel 1 (set SUMMARY=!SUMMARY! %%P:FAILED) else (set SUMMARY=!SUMMARY! %%P:ok)
  )
)
echo.
echo Result:%SUMMARY%
echo Logs in %LOGS%
