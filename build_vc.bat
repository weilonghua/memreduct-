@echo off
setlocal
call "%~dp0build.bat" --platform x64 %*
if errorlevel 1 exit /b %errorlevel%
call "%~dp0build.bat" --platform ARM64 %*
exit /b %errorlevel%
