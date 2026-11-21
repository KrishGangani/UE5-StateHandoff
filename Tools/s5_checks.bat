@echo off
rem P09 StateHandoff - the final check job: both builds, every automation test, the package, and one short
rem functional batch (B0, B2 and SH, one 30 s run each, checker on). Results: Explanation\09 (S4-S5 table).
rem Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
rem Each step is followed by an explicit ERRORLEVEL test (AGENT_BRIEF rule 12).
setlocal
echo === S5-1 editor build (StateHandoffEditor Development)
call "%~dp0build.bat" StateHandoffEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S5-2 game build (StateHandoff Development)
call "%~dp0build.bat" StateHandoff Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S5-3 automation tests StateHandoff.*
call "%~dp0run_tests.bat" StateHandoff
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S5-4 package Development
call "%~dp0package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S5-5 B0, B2, SH: one 30 s run each, N=4000, pooling on, checker on
python -I "%~dp0run_bench.py" --configs B0,B2,SH --agents 4000 --runs 1 --capture 30 --pooling 1 --no-prime --label s5_sh
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S5 checks: all steps passed
exit /b 0
