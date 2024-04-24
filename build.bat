@echo off

set SOURCES=main.cpp profiler.cpp mem.cpp tiny_fixed_alloc.cpp

set COMPILER_FLAGS= -g -O0
if ["%2"]==["release"] (
    set COMPILER_FLAGS= -g -O3
)

clang %SOURCES% %COMPILER_FLAGS% -o out.exe -Iexternal -Iexternal/mimalloc/include -march=native

IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)


if ["%1"]==["run"] (
    out.exe
)