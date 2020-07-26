@echo Copying .g3a to D: (this needs to match your mounted drive letter though) 
copy nesizm.g3a D:\ /Y
if errorlevel 1 pause
removedrive D: -L