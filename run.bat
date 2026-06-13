@echo off
setlocal enabledelayedexpansion

set BUILD_DIR=D:\Programacion\of_v0.11.2_vs2017_release\apps\myApps\guipper4
set CONFIG=Release
set PLATFORM=x64
set VS_DEVCMD="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

:: ================ COLOR SETUP ================
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do set "ESC=%%b"
set GREEN=%ESC%[92m
set RED=%ESC%[91m
set YELLOW=%ESC%[93m
set CYAN=%ESC%[96m
set WHITE=%ESC%[97m
set BOLD=%ESC%[1m
set RESET=%ESC%[0m

echo.
echo %BOLD%%CYAN%============================================%RESET%
echo %BOLD%%CYAN%   guipper4 - Build %% Run (%CONFIG% %PLATFORM%)%RESET%
echo %BOLD%%CYAN%============================================%RESET%
echo.

:: ---------- STEP 1: Setup VS environment ----------
echo %BOLD%%YELLOW%[1/4] Configurando entorno Visual Studio 2022...%RESET%
call %VS_DEVCMD% -arch=%PLATFORM%
if %ERRORLEVEL% neq 0 (
    echo %RED%[ERROR] No se pudo cargar VsDevCmd.bat%RESET%
    echo %YELLOW%  Verifica la ruta: %VS_DEVCMD%%RESET%
    pause
    exit /b 1
)
echo %GREEN%  OK - Entorno VS2022 %PLATFORM% listo%RESET%

:: ---------- STEP 2: Clean old objects ----------
echo.
echo %BOLD%%YELLOW%[2/4] Limpiando objetos compilados anteriores...%RESET%
del /f /s /q "%BUILD_DIR%\obj\%PLATFORM%\%CONFIG%\*.*" 2>nul
if exist "%BUILD_DIR%\addons\ofxNDI\src\*.obj" (
    del /f /s /q "%BUILD_DIR%\addons\ofxNDI\src\*.obj" 2>nul
)
if exist "%BUILD_DIR%\addons\ofxOsc\libs\oscpack\src\*.obj" (
    del /f /s /q "%BUILD_DIR%\addons\ofxOsc\libs\oscpack\src\*.obj" 2>nul
)
if exist "%BUILD_DIR%\addons\ofxMidi\*.obj" (
    del /f /s /q "%BUILD_DIR%\addons\ofxMidi\*.obj" 2>nul
)
echo %GREEN%  OK - Objetos limpiados%RESET%

:: ---------- STEP 3: Compile ----------
echo.
echo %BOLD%%YELLOW%[3/4] Compilando guipper4 (%CONFIG% %PLATFORM%)...%RESET%
echo.

msbuild "%BUILD_DIR%\guipper.vcxproj" ^
    /p:Configuration=%CONFIG% ^
    /p:Platform=%PLATFORM% ^
    /p:BuildProjectReferences=false ^
    /m:1 ^
    /clp:NoSummary;NoItemAndPropertyList;ErrorsOnly

set BUILD_RESULT=%ERRORLEVEL%

echo.
if %BUILD_RESULT% equ 0 (
    echo %BOLD%%GREEN%  BUILD EXITOSO - guipper4 compilado correctamente%RESET%
) else (
    echo %BOLD%%RED%  BUILD FALLIDO (codigo: %BUILD_RESULT%)%RESET%
    echo %YELLOW%  Revisa los errores arriba. Si son errores de sintaxis/LNK,%RESET%
    echo %YELLOW%  corregi los archivos y ejecuta run.bat de nuevo.%RESET%
    pause
    exit /b %BUILD_RESULT%
)

:: ---------- STEP 4: Run ----------
echo.
echo %BOLD%%YELLOW%[4/4] Ejecutando guipper4...%RESET%
echo.
echo %BOLD%%GREEN%  Lanzando: %BUILD_DIR%\bin\guipper.exe%RESET%
echo %CYAN%  (Cerro la ventana o presiona Ctrl+C en esta terminal para salir)%RESET%
echo.

cd /d "%BUILD_DIR%"
start "guipper4" /wait "%BUILD_DIR%\bin\guipper.exe"

echo.
echo %BOLD%%GREEN%  Aplicacion terminada.%RESET%
echo.
pause
