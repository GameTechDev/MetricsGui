rmdir /s /q "%~dp0sample\build"
msbuild /p:configuration=v120-nodx12-debug "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
msbuild /p:configuration=v120-nodx12-release "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
msbuild /p:configuration=v140-debug "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
msbuild /p:configuration=v140-release "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
@echo.
@echo.
@echo PASS
