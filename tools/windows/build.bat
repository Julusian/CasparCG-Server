@echo off

set BUILD_ARCHIVE_NAME=casparcg_server
set BUILD_VCVARSALL=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat
set BUILD_7ZIP=C:\Program Files\7-Zip\7z.exe
set CEF_VERSION=cef.redist.x64.3.3239.1723

:: Clean and enter shadow build folder
echo Cleaning...
if exist dist rmdir dist /s /q || goto :error
mkdir dist || goto :error

:: Setup VC++ environment
echo Setting up VC++...
call "%BUILD_VCVARSALL%" amd64 || goto :error

:: Run cmake
cd dist || goto :error
cmake -G "Visual Studio 15 2017" -A x64 ..\src || goto :error

:: Restore dependencies
echo Restore dependencies...
nuget restore || goto :error

:: Build with MSBuild
echo Building...
msbuild "CasparCG Server.sln" /t:Clean /p:Configuration=RelWithDebInfo || goto :error
msbuild "CasparCG Server.sln" /p:Configuration=RelWithDebInfo /m:%NUMBER_OF_PROCESSORS% || goto :error

:: Create server folder to later zip
set SERVER_FOLDER=casparcg_server
if exist "%SERVER_FOLDER%" rmdir "%SERVER_FOLDER%" /s /q || goto :error
mkdir "%SERVER_FOLDER%" || goto :error

:: Copy deploy resources
echo Copying deploy resources...
xcopy ..\resources\windows\flash-template-host-files "%SERVER_FOLDER%" /E /I /Y || goto :error

:: Copy binaries
echo Copying binaries...
copy shell\*.dll "%SERVER_FOLDER%" || goto :error
copy shell\RelWithDebInfo\casparcg.exe "%SERVER_FOLDER%" || goto :error
copy ..\src\shell\casparcg_auto_restart.bat "%SERVER_FOLDER%" || goto :error
copy shell\casparcg.config "%SERVER_FOLDER%" || goto :error
copy shell\*.ttf "%SERVER_FOLDER%" || goto :error
copy shell\*.pak "%SERVER_FOLDER%" || goto :error
copy shell\*.bin "%SERVER_FOLDER%" || goto :error
copy shell\*.dat "%SERVER_FOLDER%" || goto :error
xcopy packages\%CEF_VERSION%\CEF\locales "%SERVER_FOLDER%\locales" /E /I /Y || goto :error
xcopy packages\%CEF_VERSION%\CEF\swiftshader "%SERVER_FOLDER%\swiftshader" /E /I /Y || goto :error

del *_debug.dll || goto :error
del *-d-2.dll || goto :error

:: Copy documentation
echo Copying documentation...
copy ..\CHANGELOG.md "%SERVER_FOLDER%" || goto :error
copy ..\LICENSE.md "%SERVER_FOLDER%" || goto :error
copy ..\README.md "%SERVER_FOLDER%" || goto :error

:: Create zip file
echo Creating zip...
"%BUILD_7ZIP%" a "%BUILD_ARCHIVE_NAME%.zip" "%SERVER_FOLDER%" || goto :error

:: Skip exiting with failure
goto :EOF

:error
exit /b %errorlevel%
