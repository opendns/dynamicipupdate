@set PATH=%PATH%;%ProgramFiles%\NSIS

@call vc9.bat
@IF ERRORLEVEL 1 goto NO_VC9

@pushd .
@set VERSION=%1
@IF NOT DEFINED VERSION GOTO VERSION_NEEDED

@rem check if makensis exists
@makensis /version >nul
@IF ERRORLEVEL 1 goto NSIS_NEEDED

python checkver.py %VERSION%
@IF ERRORLEVEL 1 goto BAD_VERSION

devenv OpenDNSUpdater.sln /Project UpdaterUI\UpdaterUI.vcproj /ProjectConfig Release /Rebuild
@rem devenv OpenDNSUpdater.sln /Project UpdaterUI\UpdaterUI.vcproj /ProjectConfig Debug /Rebuild
@IF ERRORLEVEL 1 goto BUILD_FAILED
echo Compilation ok!

@rem TODO: should I sign the binaries as well?

@makensis /DVERSION=%VERSION% installer
@IF ERRORLEVEL 1 goto INSTALLER_FAILED

signtool sign /f opendns-sign.pfx /p bulba /d "OpenDNS Updater" /du "http://www.opendns.com/support/" /t http://timestamp.comodoca.com/authenticode OpenDNS-Updater-%VERSION%.exe
@IF ERRORLEVEL 1 goto SIGN_FAILED

@goto END

:NO_VC9
echo vc9.bat failed
@goto END

:INSTALLER_FAILED
echo Installer script failed
@goto END

:BUILD_FAILED
echo Build failed!
@goto END

:VERSION_NEEDED
echo Need to provide version number e.g. build-release.bat 1.0
@goto END

:NSIS_NEEDED
echo NSIS doesn't seem to be installed. Get it from http://nsis.sourceforge.net/Download
@goto END

:SIGN_FAILED
@echo Failed to sign the installer
@goto END

:BAD_VERSION
@goto END

:END
@popd

