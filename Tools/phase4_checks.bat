@echo off
rem P09 StateHandoff - all Phase 4 foundation checks in one queue job, in order; stops at the first failure.
rem F-1 editor build, F-2 game build, F-3 uncooked smoke, F-4 package Development, F-5 packaged smoke (no rendering).
rem Results and their log checks are recorded in Explanation\09_Testing_And_Benchmarks.md (Phase 4 table).
rem Run it through the machine-wide UE job queue (tools\ue_queue.py), never directly.
setlocal
echo === F-1 editor build (StateHandoffEditor Development)
call "%~dp0build.bat" StateHandoffEditor Development || exit /b 1
echo === F-2 game build (StateHandoff Development)
call "%~dp0build.bat" StateHandoff Development || exit /b 2
echo === F-3 uncooked smoke (editor binary, -game -nullrhi)
call "%~dp0smoke_game.bat" || exit /b 3
echo === F-4 package Development
call "%~dp0package.bat" Development || exit /b 4
echo === F-5 packaged smoke (-nullrhi)
call "%~dp0smoke_packaged.bat" || exit /b 5
echo === Phase 4 checks: all steps exited with code 0
exit /b 0
