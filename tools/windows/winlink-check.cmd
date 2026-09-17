@echo off
rem  The two-half programs of tests\winlink, each linked across the boundary
rem  both ways: the a-half by cxx1 and the b-half by cl, then the reverse.
rem  Usage:  winlink-check.cmd <root>
rem  Writes  <root>\winout\winlink-<name>-ab.out and -ba.out for a program
rem  that ran, and a line naming the step for one that did not; the
rem  comparison with <name>.expected is tools/verify-three's, on the Mac,
rem  for the line-ending reason run-cases.cmd gives.
setlocal enabledelayedexpansion
if "%~1"=="" (echo winlink-check.cmd: needs the tree root & exit /b 2)
set ROOT=%~1
set WORK=%ROOT%\winlink
if not exist %WORK% mkdir %WORK%
del /q %WORK%\* 2>nul
if not exist %ROOT%\winout mkdir %ROOT%\winout

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (echo winlink-check: no vcvars64 & exit /b 1)

for %%f in (%ROOT%\tests\winlink\*.expected) do (
    set NAME=%%~nf
    set SRC=%ROOT%\tests\winlink
    rem  a by cxx1, b by cl
    %ROOT%\cxx1-msvc.exe -c !SRC!\!NAME!-a.cpp -o %WORK%\!NAME!-a1.obj >%WORK%\!NAME!-ab.log 2>&1
    if errorlevel 1 (echo WINLINK-FAILED !NAME!-ab: cxx1 refused the a-half) else (
        cl /nologo /EHsc /c /I!SRC! /Fo%WORK%\!NAME!-b1.obj !SRC!\!NAME!-b.cpp >>%WORK%\!NAME!-ab.log 2>&1
        if errorlevel 1 (echo WINLINK-FAILED !NAME!-ab: cl refused the b-half) else (
            link /nologo /OUT:%WORK%\!NAME!-ab.exe %WORK%\!NAME!-a1.obj %WORK%\!NAME!-b1.obj >>%WORK%\!NAME!-ab.log 2>&1
            if errorlevel 1 (echo WINLINK-FAILED !NAME!-ab: the link failed) else (
                %WORK%\!NAME!-ab.exe > %ROOT%\winout\winlink-!NAME!-ab.out 2>&1
            )
        )
    )
    rem  b by cxx1, a by cl
    %ROOT%\cxx1-msvc.exe -c !SRC!\!NAME!-b.cpp -o %WORK%\!NAME!-b2.obj >%WORK%\!NAME!-ba.log 2>&1
    if errorlevel 1 (echo WINLINK-FAILED !NAME!-ba: cxx1 refused the b-half) else (
        cl /nologo /EHsc /c /I!SRC! /Fo%WORK%\!NAME!-a2.obj !SRC!\!NAME!-a.cpp >>%WORK%\!NAME!-ba.log 2>&1
        if errorlevel 1 (echo WINLINK-FAILED !NAME!-ba: cl refused the a-half) else (
            link /nologo /OUT:%WORK%\!NAME!-ba.exe %WORK%\!NAME!-b2.obj %WORK%\!NAME!-a2.obj >>%WORK%\!NAME!-ba.log 2>&1
            if errorlevel 1 (echo WINLINK-FAILED !NAME!-ba: the link failed) else (
                %WORK%\!NAME!-ba.exe > %ROOT%\winout\winlink-!NAME!-ba.out 2>&1
            )
        )
    )
)
echo winlink-check.cmd: done
endlocal
