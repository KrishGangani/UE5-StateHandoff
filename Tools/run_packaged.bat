@echo off
rem P09 StateHandoff - run the PACKAGED game (the real executable, not the bootstrap) with the given arguments, until it quits.
rem Usage: set "SH_LOG=<name>" ^& run_packaged.bat [args...]   e.g. run_packaged.bat -SHAgents=1000 -SHCapture=4 -SHExit
rem The log goes to StateHandoff\Saved\Logs\%SH_LOG%.log (default name: Packaged), named by an environment variable
rem for the same reason as run_uncooked.bat. Functional checks; timed runs go through run_bench.py (pinned to P-cores).
rem Needs package.bat first. Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
setlocal
set "EXE=%~dp0..\Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe"
if "%SH_LOG%"=="" set "SH_LOG=Packaged"
set "LOG=%~dp0..\StateHandoff\Saved\Logs\%SH_LOG%.log"
if not exist "%EXE%" (
  echo run_packaged: "%EXE%" not found. Run package.bat first.
  exit /b 2
)

"%EXE%" -nosound -unattended -nosplash -traceautostart=0 -log -abslog="%LOG%" %*
exit /b %ERRORLEVEL%
