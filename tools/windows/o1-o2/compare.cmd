@echo off
rem  cxx1 -O1/-O2 against cl /O1 and /O2 on the sixteen Compiler++ units, on
rem  the box: .text, run time, and every runnable case's output compared.
rem
rem    compare.cmd <tree root>      (after the tree is relayed to C:\cxx1\verify)
rem
rem  Run by its full path, no `cmd /c`, with `ssh -n`. Builds cxx1-msvc.exe
rem  first; the cl-built references are C:\o12study\b-O1 and b-O2 from study.cmd.
setlocal
if "%~1"=="" (echo compare.cmd: needs the tree root & exit /b 2)
set ROOT=%~1
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "GITBASH=C:\Program Files\Git\bin\bash.exe"
( call "%VCVARS%" >nul ) < NUL
if errorlevel 1 ( echo compare: vcvars64.bat failed & exit /b 1 )
call %ROOT%\msvc\build.cmd
if errorlevel 1 exit /b 1
set CPP11_AS=C:\masm-tests\build\asm-win.exe
"%GITBASH%" -c "sh $(cygpath -u '%ROOT%')/tools/windows/o1-o2/compare.sh '%ROOT%'" < NUL
exit /b %errorlevel%
