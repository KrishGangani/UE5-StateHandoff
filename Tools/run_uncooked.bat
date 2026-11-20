@echo off
rem P09 StateHandoff - run the uncooked game (editor binary, -game, no rendering) with the given arguments, until it quits.
rem Usage: set "SH_LOG=<name>" ^& run_uncooked.bat [args...]   e.g. run_uncooked.bat -SHAgents=4000 -SHCapture=20 -SHExit
rem Set SH_LOG before EVERY run: it stays set in the calling shell, so a later run would reuse the old name.
rem The log goes to StateHandoff\Saved\Logs\%SH_LOG%.log (default name: Run). The log name comes from an environment
rem variable because cmd splits "=" inside %%1, so -SHAgents=4000 cannot follow a positional argument.
rem Functional checks only (never benchmarks: the editor binary and -nullrhi). Run it through the UE job queue.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\StateHandoff\StateHandoff.uproject"
if "%SH_LOG%"=="" set "SH_LOG=Run"
set "LOG=%~dp0..\StateHandoff\Saved\Logs\%SH_LOG%.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -log -abslog="%LOG%" %*
exit /b %ERRORLEVEL%
