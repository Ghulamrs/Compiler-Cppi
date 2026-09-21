@echo off
rem  What /O1 and /O2 are, asked of cl and cl6x on the box - see
rem  docs/O1-O2-STUDY-2026-09-21.md for what it found.
rem
rem    scp -r tools/windows/o1-o2 windows:C:/o12study
rem    ssh -n windows "C:\o12study\study.cmd"
rem    scp windows:C:/o12study/results.tar .
rem
rem  Run by its full path, no `cmd /c`, and with `ssh -n`: vcvars reads stdin.
rem  vcvars first, then the whole study under Git bash, the one POSIX shell here.
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "GITBASH=C:\Program Files\Git\bin\bash.exe"
( call "%VCVARS%" >nul ) < NUL
if errorlevel 1 ( echo study: vcvars64.bat failed & exit /b 1 )
"%GITBASH%" -c "sh /c/o12study/study.sh" < NUL
exit /b %errorlevel%
