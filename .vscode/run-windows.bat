cmake --build build
IF %ERRORLEVEL% NEQ 0 (Exit /b 1)
dos\tools\dosbox-x\dosbox-x -conf dos\tools\dosbox-x.conf -fastlaunch -exit %1.exe
IF %ERRORLEVEL% NEQ 0 (Exit /b 1)
