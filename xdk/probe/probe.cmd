@echo off
rem Spire360 dialect probe. Run from the "Xbox 360 Command Prompt" (cl.exe on
rem PATH is the XDK's PowerPC compiler). Compiles each probe file on its own
rem and writes probe_results.txt: PASS = the feature compiles.
setlocal enabledelayedexpansion
set OUT=%~dp0probe_results.txt
set OBJ=%TEMP%\spire360_probe
if not exist "%OBJ%" mkdir "%OBJ%"
echo Spire360 dialect probe > "%OUT%"
cl 2>&1 | findstr /i "version" >> "%OUT%"
for %%F in ("%~dp0c_*.c") do (
  cl /nologo /c /TC /W3 /D_XBOX /I"%~dp0..\..\third_party\w2c2\w2c2" "%%F" /Fo"%OBJ%\%%~nF.obj" > "%OBJ%\%%~nF.log" 2>&1
  if !errorlevel! equ 0 (echo PASS %%~nF>> "%OUT%") else (echo FAIL %%~nF>> "%OUT%" & type "%OBJ%\%%~nF.log" >> "%OUT%")
)
for %%F in ("%~dp0cpp_*.cpp") do (
  cl /nologo /c /TP /W3 /D_XBOX /EHs-c- "%%F" /Fo"%OBJ%\%%~nF.obj" > "%OBJ%\%%~nF.log" 2>&1
  if !errorlevel! equ 0 (echo PASS %%~nF>> "%OUT%") else (echo FAIL %%~nF>> "%OUT%" & type "%OBJ%\%%~nF.log" >> "%OUT%")
)
type "%OUT%"
