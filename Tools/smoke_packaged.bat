@echo off
rem P09 StateHandoff - smoke test of the PACKAGED game: start the real executable (not the bootstrap), load the map, quit.
rem Usage: smoke_packaged.bat [extra args...]   e.g. smoke_packaged.bat -windowed -ResX=1280 -ResY=720   (default: no rendering)
rem Needs package.bat first. Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
setlocal
set "EXE=%~dp0..\Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe"
set "LOG=%~dp0..\StateHandoff\Saved\Logs\SmokePackaged.log"
if not exist "%EXE%" (
  echo smoke_packaged: "%EXE%" not found. Run package.bat first.
  exit /b 2
)

"%EXE%" -nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExecCmds="quit" -log -abslog="%LOG%" %*
exit /b %ERRORLEVEL%
