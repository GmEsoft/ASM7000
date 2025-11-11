::call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"
call "vcvars32.bat"
cl /EHsc ASM7000.cpp
if errorlevel 1 cl /EHsc ASM7000.cpp >ASM7000.out
