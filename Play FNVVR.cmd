@echo off
setlocal
cd /d "%~dp0"
echo Connect Quest Link or Air Link before launching FNVVR.
echo Loading the existing Goodsprings save past Doc Mitchell's opening.
echo Use the game's Quit menu when finished so the launcher can restore its temporary files.
echo.
if not exist "%~dp0local\product-build\fnvxr-product-Release.json" (
    echo Preparing the current FNVVR build attestation...
    powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-fnvxr-product.ps1" -Incremental -Focused
    if errorlevel 1 (
        echo.
        echo FNVVR could not prepare its build. Details are above.
        pause
        exit /b 1
    )
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-fnvxr-product.ps1" -GameRoot "%~dp0local\retail-sandbox-v1" -UseAttestedBuild -PhysicalHeadsetPlay -PhysicalRuntimeManifest "C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json" -RetailFixtureAction Load -RetailFixtureWeapon Pistol -RetailReadyTimeoutSeconds 120 -HostReadyTimeoutSeconds 90
if errorlevel 1 (
    echo.
    echo FNVVR could not start or the session ended with an error. Details are above.
    pause
)
endlocal
