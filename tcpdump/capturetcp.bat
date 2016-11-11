cd /d Z:/people/Abel/arena-automation/data/tcp/
set datetimef=%date:~-4%-%date:~3,2%-%date:~0,2% %time:~0,2%-%time:~3,2%-%time:~6,2%
"C:\Program Files\Wireshark\dumpcap.exe" -b filesize:1048576 -w "%datetimef%.cap" 
pause
