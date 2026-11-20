@echo off
rem P09 StateHandoff - run the automation tests headless in the editor and exit with their result.
rem Usage: run_tests.bat [Filter]   (default filter: StateHandoff, i.e. every test of this project)
rem Exit code 0 = all passed; non-zero = a failure. Needs the Development editor target (build.bat).
rem Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\StateHandoff\StateHandoff.uproject"
set "FILTER=%~1"
if "%FILTER%"=="" set "FILTER=StateHandoff"
set "REPORT=%~dp0..\StateHandoff\Saved\Automation\Reports"
set "LOG=%~dp0..\StateHandoff\Saved\Logs\Tests.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -ExecCmds="Automation RunTests %FILTER%;Quit" -unattended -nullrhi -nosound -nosplash -nopause -log -abslog="%LOG%" -ReportExportPath="%REPORT%"
exit /b %ERRORLEVEL%
