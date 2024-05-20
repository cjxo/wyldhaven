@echo off

set CompilerOpts=/nologo /Zi /Oi /W4 /wd4505 /wd4201 /wd4127 /Zc:strictStrings- /Od
set CompilerDefs=/DWC_DEBUG
set LinkerLibs=user32.lib D3D11.lib d3dcompiler.lib DXGI.lib dxguid.lib freetype.lib Winmm.lib

if not exist ..\build mkdir ..\build
pushd ..\build

rem cl %CompilerOpts% /LD %CompilerDefs% /I..\code\ext\freetype ..\code\wyld_game.c user32.lib D3D11.lib d3dcompiler.lib DXGI.lib dxguid.lib freetype.lib Winmm.lib /link /PDB:wyld_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.pdb /incremental:no
cl %CompilerOpts% %CompilerDefs% /I..\code\ext\freetype ..\code\wyld_main.c user32.lib D3D11.lib d3dcompiler.lib DXGI.lib dxguid.lib freetype.lib Winmm.lib /link /incremental:no
popd
