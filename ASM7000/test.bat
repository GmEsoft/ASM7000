@echo off
set CMD=ASM7000 -i:TEST -o:TEST -l:TEST -nc -nh -nn
echo %CMD%
%CMD%

fc /b /a TEST.cim TEST.bin
