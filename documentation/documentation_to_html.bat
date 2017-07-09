@echo off
:loop  
C:/Windows/System32/timeout /t 2
pandoc --number-sections --toc --toc-depth=6 --email-obfuscation=javascript --to html+auto_identifiers+multiline_tables --self-contained --output="Arena Automation System Documentation.html" --css "Arena Automation System Documentation.css" "Arena Automation System Documentation.md"
goto :loop
