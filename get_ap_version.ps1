$line = Get-Content CMakeLists.txt | Select-String '^set\(AP_RELEASE_VERSION' | Select-Object -First 1
($line.ToString().Split('"'))[1]
