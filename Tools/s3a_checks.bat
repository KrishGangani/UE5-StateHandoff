@echo off
rem P09 StateHandoff - all S3a checks in one queue job, in order; stops at the first failure.
rem S3a-1/2 builds (0 warnings: checked in the queue log), S3a-3 automation tests, S3a-4 csv_stats self-test,
rem S3a-5 package, S3a-6/7 B3 (entity-owned) must show no incidents with pooling off and on, S3a-8..10 B0-B2 with
rem pooling off, on, and on with bKeepLowResActors off (H2). Small run_bench.py batches: functional checks, not measurements.
rem Results and their log checks are recorded in Explanation\09_Testing_And_Benchmarks.md (S3a table).
rem Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
rem Each step is followed by an explicit ERRORLEVEL test: "call x || exit /b" can exit with 0 after a failure
rem under cmd /c (found by UE5-NavReadyPCG; AGENT_BRIEF rule 12). The test also passes negative crash codes on.
setlocal
echo === S3a-1 editor build (StateHandoffEditor Development)
call "%~dp0build.bat" StateHandoffEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-2 game build (StateHandoff Development)
call "%~dp0build.bat" StateHandoff Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-3 automation tests StateHandoff.*
call "%~dp0run_tests.bat" StateHandoff
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-4 csv_stats.py self-test
python -I "%~dp0csv_stats.py" --selftest
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-5 package Development
call "%~dp0package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-6 B3, pooling off: expects no incidents (with the batch's priming run)
python -I "%~dp0run_bench.py" --configs B3 --agents 4000 --runs 1 --capture 30 --label s3a_b3p0 --expect-incidents none
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-7 B3, pooling on: expects no incidents
python -I "%~dp0run_bench.py" --configs B3 --agents 4000 --runs 1 --capture 30 --pooling 1 --no-prime --label s3a_b3p1 --expect-incidents none
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-8 B0, B1, B2, pooling off
python -I "%~dp0run_bench.py" --configs B0,B1,B2 --agents 4000 --runs 1 --capture 30 --no-prime --label s3a_p0
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-9 B0, B1, B2, pooling on
python -I "%~dp0run_bench.py" --configs B0,B1,B2 --agents 4000 --runs 1 --capture 30 --pooling 1 --no-prime --label s3a_p1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a-10 B0, B1, B2, pooling on, bKeepLowResActors off (H2 variant)
python -I "%~dp0run_bench.py" --configs B0,B1,B2 --agents 4000 --runs 1 --capture 30 --pooling 1 --keep-low-res 0 --no-prime --label s3a_k0
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo === S3a checks: all steps passed
exit /b 0
