@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /std:c++17 /EHsc /O2 /I. /IAudioDSPTools /INeuralAmpModeler experiments\tone_stack_probe.cpp /Fe:experiments\tone_stack_probe.exe
