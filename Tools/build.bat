@echo off
rem P09 StateHandoff - build one target from the command line (reproducible; no IDE needed).
rem Usage: build.bat [Target] [Configuration]
rem   Target:        StateHandoffEditor (default) | StateHandoff
rem   Configuration: Development (default) | Shipping | DebugGame   (Test: not available in the launcher engine)
rem Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\StateHandoff\StateHandoff.uproject"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=StateHandoffEditor"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE
exit /b %ERRORLEVEL%
