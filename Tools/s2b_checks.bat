@echo off
rem P09 StateHandoff - all S2b checks in one queue job, in order; stops at the first failure.
rem S2b-1/2 builds (0 warnings: checked in the queue log), S2b-3 automation tests, S2b-4 csv_stats self-test,
rem S2b-5 package, S2b-6 a configuration not implemented yet is refused, S2b-7..9 three small run_bench.py batches.
rem Results and their log checks are recorded in Explanation\09_Testing_And_Benchmarks.md (S2b table).
rem Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
rem Each step is followed by an explicit ERRORLEVEL test: "call x || exit /b" can exit with 0 after a failure
rem under cmd /c (found by UE5-NavReadyPCG; AGENT_BRIEF rule 12). The test also passes negative crash codes on.
setlocal
echo === S2b-1 editor build (StateHandoffEditor Development)
call "%~dp0build.bat" StateHandoffEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b-2 game build (StateHandoff Development)
call "%~dp0build.bat" StateHandoff Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b-3 automation tests StateHandoff.*
call "%~dp0run_tests.bat" StateHandoff
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b-4 csv_stats.py self-test
python -I "%~dp0csv_stats.py" --selftest
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b-5 package Development
call "%~dp0package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b-6 -SHConfig=SH is refused until S5 (expects exit code 3)
set "SH_LOG=S2b_RefuseSH"
call "%~dp0run_packaged.bat" -nullrhi -SHConfig=SH -SHAgents=100 -SHExit
if %ERRORLEVEL% neq 3 (
  echo S2b-6: expected exit code 3, got %ERRORLEVEL%
  exit /b 6
)
echo === S2b-7 B0, orbit and flips, checker on: expects incidents (with the batch's priming run)
python -I "%~dp0run_bench.py" --configs B0 --agents 4000 --runs 1 --capture 30 --label s2b_orbit --expect-incidents some
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b-8 B0, camera parked, checker on: expects no incidents
python -I "%~dp0run_bench.py" --configs B0 --agents 4000 --runs 1 --capture 30 --parked --no-prime --label s2b_park --expect-incidents none
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b-9 B0, checker off (timing mode): the run completes with its capture and report
python -I "%~dp0run_bench.py" --configs B0 --agents 4000 --runs 1 --capture 30 --check 0 --no-prime --label s2b_nocheck
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S2b checks: all steps passed
exit /b 0
