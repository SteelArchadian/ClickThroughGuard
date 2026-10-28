@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
cl /nologo /O2 /EHsc /MT /LD /DUNICODE /D_UNICODE /W3 ClickThroughGuard.cpp user32.lib /Fe:ClickThroughGuard.dll /link /NOLOGO
