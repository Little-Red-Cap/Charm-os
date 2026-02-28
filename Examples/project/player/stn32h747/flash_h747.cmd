@echo off
setlocal

set OPENOCD=D:\Toolchains\OpenOCD\bin\openocd.exe
set OPENOCD_SCRIPTS=D:\Toolchains\OpenOCD\share\openocd\scripts
set CFG=G:\Project\Charm-os\Examples\project\player\stn32h747\openocd_swd.cfg
set ELF=G:\Project\Charm-os\Examples\project\player\stn32h747\CM7\cmake-build-debug\stn32h747_CM7.elf

"%OPENOCD%" -s "%OPENOCD_SCRIPTS%" -f "%CFG%" ^
  -c "tcl_port disabled" ^
  -c "gdb_port disabled" ^
  -c "tcl_port disabled" ^
  -c "program %ELF%" ^
  -c reset ^
  -c shutdown

endlocal
