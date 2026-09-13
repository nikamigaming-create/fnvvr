@echo off
setlocal
cd /d "%~dp0"
echo Connect Quest Link or Air Link before launching FNVVR.
echo Loading the existing Goodsprings save past Doc Mitchell's opening.
echo Use the game's Quit menu when finished so the launcher can restore its temporary files.
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-fnvxr-product.ps1" -GameRoot "%~dp0local\retail-sandbox-v1" -UseAttestedBuild -PhysicalHeadsetPlay -PhysicalRuntimeManifest "C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json" -RetailFixtureAction Load -RetailFixtureWeapon Pistol -RetailReadyTimeoutSeconds 120 -HostReadyTimeoutSeconds 90
if errorlevel 1 (
    echo.
    echo FNVVR could not start or the session ended with an error. Details are above.
    pause
)
endlocal
