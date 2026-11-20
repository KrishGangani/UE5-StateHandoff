@echo off
rem P09 StateHandoff - package a standalone Win64 build (BuildCookRun).
rem Usage: package.bat [Configuration]   Configuration: Development (default; headline benchmarks) | Shipping (no CSV profiler; not used)
rem Note: the launcher (installed) engine cannot build the Test configuration.
rem Output: ..\Build\Windows\StateHandoff.exe (bootstrap) and ..\Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe (real game)
rem Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\StateHandoff\StateHandoff.uproject"
set "ARCHIVE=%~dp0..\Build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJECT%" -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory="%ARCHIVE%"
exit /b %ERRORLEVEL%
