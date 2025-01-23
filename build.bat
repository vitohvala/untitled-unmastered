@echo off

if not exist build mkdir build
pushd build

set CommonCompilerFlags=-nologo -FC -WX -W4 -wd4201 -wd4189 -MT -Oi -Od -GR- -Gm- -EHa- -Zi

REM  -nologo Suppress startup banner -Od Disable all optimizations -Oi Generate Intrinsic Functions -W3, -W4, -Wall, -wd, -WX Warning-related switches -Zi, -Z7 Debug Info Format 

cl %CommonCompilerFlags% ../src/untitled.c -LD /link -EXPORT:untitled_update_game -incremental:no  
cl %CommonCompilerFlags% ../src/win32_untitled.c  user32.lib gdi32.lib winmm.lib

popd
