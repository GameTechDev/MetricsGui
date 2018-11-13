rmdir /s /q "%~dp0sample\build"
msbuild /p:configuration=debug-nodx12 "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
msbuild /p:configuration=release-nodx12 "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
msbuild /p:configuration=debug "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
msbuild /p:configuration=release "%~dp0sample\sample.sln"
@if not "%errorlevel%"=="0" exit /b 1
@echo.
@echo.
@echo PASS
