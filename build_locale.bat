@echo off
setlocal
python "%~dp0tools\build_locale.py"
exit /b %errorlevel%
