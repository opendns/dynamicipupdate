@set PATH=%PATH%;%ProgramFiles%\NSIS

@call vc9.bat
@IF ERRORLEVEL 1 goto NO_VC9

@rem @pushd .

@rem check if makensis exists
@makensis /version >nul
@IF ERRORLEVEL 1 goto NSIS_NEEDED

python buildandupload.py
@IF ERRORLEVEL 1 goto BUILD_FAILED

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

:NSIS_NEEDED
echo NSIS doesn't seem to be installed. Get it from http://nsis.sourceforge.net/Download
@goto END

:END
@popd

