@echo off
rem 3-stage self-host build without MSVC: stage 1 by a seed tcc, stage 2 by
rem stage 1, stage 3 by stage 2; stages 2 and 3 must be byte-identical.
rem usage: tests\jitbench\bootstrap.cmd [seed-tcc.exe]   (default: tcc.exe on PATH)
rem Leaves the stage 3 build in win32\. Stage copies and logs go to %TEMP%\tcc-bootstrap.
setlocal
set SEED=%~1
if "%SEED%"=="" for %%i in (tcc.exe) do set SEED=%%~$PATH:i
if "%SEED%"=="" echo bootstrap.cmd: no seed tcc.exe given or on PATH& exit /b 1
set S=%TEMP%\tcc-bootstrap\
cd /d "%~dp0..\..\win32" || exit /b 1
if exist "%S%" rmdir /s /q "%S%"
mkdir "%S%"
call .\build-tcc.bat -clean >nul
call .\build-tcc.bat -c "%SEED%" -t x86_64 > "%S%stage1.log" 2>&1
echo stage1 exit=%ERRORLEVEL%
xcopy /s /i /q /y . "%S%stage1" >nul
call .\build-tcc.bat -clean >nul
call .\build-tcc.bat -c "%S%stage1\tcc.exe" -t x86_64 > "%S%stage2.log" 2>&1
echo stage2 exit=%ERRORLEVEL%
xcopy /s /i /q /y . "%S%stage2" >nul
call .\build-tcc.bat -clean >nul
call .\build-tcc.bat -c "%S%stage2\tcc.exe" -t x86_64 > "%S%stage3.log" 2>&1
echo stage3 exit=%ERRORLEVEL%
set RC=0
for %%f in (tcc.exe libtcc.dll lib\libtcc1.a) do (
  fc /b %%f "%S%stage2\%%f" >nul && (echo %%f: stage 2 == stage 3, %%~zf bytes) || (echo %%f DIFFERS between stage 2 and stage 3& set RC=1)
)
.\tcc.exe -v
exit /b %RC%
