@echo off
rem xdk\build.cmd with fewer compiler processes: at most SPIRE_JOBS at a
rem time (default 2) instead of one per core, so the PC stays usable. The
rem projects pass it to cl as /MPn (xdk\common\spire360.props).
rem
rem   xdk\build_light.cmd spire360     two compilers at a time
rem
rem Another cap: "set SPIRE_JOBS=3" first ("set SPIRE_JOBS=" for 2 again).
rem logic_lib's longest file, logic.c, is one compiler process either way;
rem the other files compile beside it. Switching between this and
rem build.cmd changes the compiler command line, so logic_lib compiles in
rem full once (as it does when the logic changes).
setlocal
if not defined SPIRE_JOBS set SPIRE_JOBS=2
echo Compiling with at most %SPIRE_JOBS% compiler processes at a time
call "%~dp0build.cmd" %*
exit /b %errorlevel%
