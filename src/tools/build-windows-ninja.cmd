@echo off
setlocal EnableExtensions

rem ===========================================================================
rem build-windows-ninja.cmd - Windows Ninja + MSVC build entry.
rem
rem Pins the console code page to 936 (GBK) before both configure and build.
rem CMake detects the MSVC /showIncludes dependency prefix at configure time
rem (CMAKE_C/CXX_CL_SHOWINCLUDES_PREFIX) by decoding cl output with the
rem console output code page, and Ninja later matches that prefix byte-wise
rem against cl output.  If configure and build run under a different effective
rem code page (for example a UTF-8 PowerShell session), the prefix written to
rem rules.ninja is mojibake, Ninja records no header dependencies, and stale
rem object files are not rebuilt.  Running both steps from this script keeps
rem the code page consistent.
rem
rem Usage:
rem   build-windows-ninja.cmd [build-dir] [Debug|Release|RelWithDebInfo]
rem
rem Environment overrides:
rem   NEUROLINGSCE_BUILD_CODEPAGE
rem             console code page used for configure and build
rem             (default: 936 for zh-CN Windows; override for other locales)
rem   QT6_DIR   Qt6 CMake config directory
rem             (default: D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6)
rem   VCVARS64  explicit path to vcvars64.bat; otherwise vswhere / common
rem             VS 2022 locations are probed.
rem   NEUROLINGSCE_STORE_PROFILE
rem             custom (default), staging, or disabled
rem   NEUROLINGSCE_MASCOT_INDEX_URL / NEUROLINGSCE_SUBMISSION_SERVICE_URL /
rem   NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID
rem             public Store configuration forwarded to CMake; values are
rem             never inferred from gh CLI authentication.
rem ===========================================================================

if not defined NEUROLINGSCE_BUILD_CODEPAGE set "NEUROLINGSCE_BUILD_CODEPAGE=936"
chcp %NEUROLINGSCE_BUILD_CODEPAGE% >nul
if errorlevel 1 (
  echo build-windows-ninja.cmd: chcp %NEUROLINGSCE_BUILD_CODEPAGE% failed. 1>&2
  exit /b 1
)

set "REPO_ROOT=%~dp0..\.."
for %%I in ("%REPO_ROOT%") do set "REPO_ROOT=%%~fI"

set "BUILD_DIR=%~1"
if "%BUILD_DIR%"=="" set "BUILD_DIR=%REPO_ROOT%\build"
set "BUILD_CONFIG=%~2"
if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Debug"

if not defined QT6_DIR set "QT6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6"

rem ---- Locate the MSVC developer environment --------------------------------
if defined VCVARS64 goto :have_vcvars

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
  if defined VSINSTALL (
    set "VCVARS64=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
    if exist "%VCVARS64%" goto :have_vcvars
  )
)

for %%V in (
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise"
) do (
  if exist "%%~V\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS64=%%~V\VC\Auxiliary\Build\vcvars64.bat"
    goto :have_vcvars
  )
)

echo build-windows-ninja.cmd: unable to locate vcvars64.bat. Set VCVARS64. 1>&2
exit /b 1

:have_vcvars
call "%VCVARS64%" >nul
if errorlevel 1 exit /b %errorlevel%

set "STORE_CONFIG_ARGS="
if defined NEUROLINGSCE_STORE_PROFILE set "STORE_CONFIG_ARGS=%STORE_CONFIG_ARGS% -DNEUROLINGSCE_STORE_PROFILE=%NEUROLINGSCE_STORE_PROFILE%"
if defined NEUROLINGSCE_MASCOT_INDEX_URL set "STORE_CONFIG_ARGS=%STORE_CONFIG_ARGS% -DNEUROLINGSCE_MASCOT_INDEX_URL=%NEUROLINGSCE_MASCOT_INDEX_URL%"
if defined NEUROLINGSCE_SUBMISSION_SERVICE_URL set "STORE_CONFIG_ARGS=%STORE_CONFIG_ARGS% -DNEUROLINGSCE_SUBMISSION_SERVICE_URL=%NEUROLINGSCE_SUBMISSION_SERVICE_URL%"
if defined NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID set "STORE_CONFIG_ARGS=%STORE_CONFIG_ARGS% -DNEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID=%NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID%"

rem Configure (regenerates rules.ninja under the pinned code page) and build.
cmake -S "%REPO_ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_CONFIG% -DQt6_DIR=%QT6_DIR% %STORE_CONFIG_ARGS%
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --parallel
exit /b %errorlevel%
