@set PATH=%PATH%;%ProgramFiles%\NSIS

@call vc9.bat
@IF ERRORLEVEL 1 goto NO_VC9

@rem @pushd .
@rem @set VERSION=%1
@rem @IF NOT DEFINED VERSION GOTO VERSION_NEEDED

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

:VERSION_NEEDED
echo Need to provide version number e.g. build-release.bat 1.0
@goto END

:NSIS_NEEDED
echo NSIS doesn't seem to be installed. Get it from http://nsis.sourceforge.net/Download
@goto END

:BUILD_FAILED
@rem @echo Failed to build
@goto END

:BAD_VERSION
@goto END

:END
@popd

