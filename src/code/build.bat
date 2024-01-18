@echo off

setLocal

REM This build has to be run from vim in the root of the project

where /q cl || (
  echo ERROR: "cl" not found - please run this from the MSVC x64 native tools command prompt.
  exit /b 1
)

if "%Platform%" neq "x64" (
    echo ERROR: Platform is not "x64" - please run this from the MSVC x64 native tools command prompt.
    exit /b 1
)

set CommonCompilerFlags= -nologo -Z7 
REM set CommonLinkerFlags= Gdi32.lib User32.lib

if not exist .\build mkdir .\build
pushd build

echo -------------------------------------
echo Building with MSVC
echo -------------------------------------

REM HEADS UP: This time counter might cause errors in other systems.
for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set startdate=%%b-%%a-%%c)
for /f "tokens=1-3 delims=/:" %%a in ("%TIME%")  do (set starttime=%%a:%%b:%%c)
@echo Compilation started %startdate% at %starttime%
@echo:

REM del *.pbd > NUL 2> NUL
REM echo WAITING FOR PBD > lock.tmp
cl %CommonCompilerFlags% ..\src\code\main.cpp  /link -incremental:no -opt:ref %CommonLinkerFlags%
REM del lock.tmp

for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set finishdate=%%b-%%a-%%c)
for /f "tokens=1-3 delims=/:" %%a in ("%TIME%") do (set finishtime=%%a:%%b:%%c)
@echo:
@echo Compilation finished %finishdate% at %finishtime%

popd

