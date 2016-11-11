cd /d Z:\people\Abel\arena-automation\data\tcp
"C:\Program Files\Wireshark\tshark.exe" -r test.cap -T fields -e ip.addr -e data > test.txt
pause
