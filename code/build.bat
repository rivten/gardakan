@echo off

ctime -begin gardakan.ctm

set SDLPath=C:\SDL2-2.0.4
set RivtenPath=..\..\rivten\

set UntreatedWarnings=/wd4100 /wd4244 /wd4201 /wd4127 /wd4505 /wd4456 /wd4996
set CommonCompilerFlags=/MT /Od /Oi /fp:fast /fp:except- /Zo /Gm- /GR- /EHa /WX /W4 %UntreatedWarnings% /Z7 /nologo /I %SDLPath%\include /I %RivtenPath%
set CommonLinkerFlags=/incremental:no /opt:ref /subsystem:console /ignore:4099 %SDLPath%\lib\x64\SDL2.lib %SDLPath%\lib\x64\SDL2_net.lib

pushd ..\build\
cl %CommonCompilerFlags% ..\code\gardakan.cpp /link %CommonLinkerFlags%
popd

ctime -end gardakan.ctm
