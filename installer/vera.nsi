	RequestExecutionLevel admin

Name "VERA 0.3"
outfile "vera0.3_install.exe"


installDir $PROGRAMFILES\JERKS

	DirText "To install VERA 0.3, please choose a directory."

# open section
section


	setOutPath $INSTDIR

	file .\installme.txt
	WriteUninstaller $INSTDIR\vera0.3_uninstall.exe

	CreateDirectory "$SMPROGRAMS\VERA"
	CreateShortCut "$SMPROGRAMS\VERA\VERA 0.3.lnk" "$INSTDIR\installme.txt"
	CreateShortCut "$DESKTOP\VERA 0.3.lnk" "$INSTDIR\installme.txt"

sectionEnd




Section "Uninstall"

	Delete $INSTDIR\vera0.3_uninstall.exe
	Delete $INSTDIR\installme.txt
	RMDIR $INSTDIR

	Delete "$SMPROGRAMS\VERA\VERA 0.3.lnk"
	RMDIR "$SMPROGRAMS\VERA\"

	Delete "$DESKTOP\VERA 0.3.lnk"
SectionEnd