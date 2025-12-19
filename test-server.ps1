$curl = "curl.exe"  # usa el curl real (Git o Windows 10+)

Write-Host "`n=== Test 2: GET /stream (5s) ===" -ForegroundColor Green
& $curl -v -N --max-time 5 http://localhost:8080/stream

Write-Host "=== Test 1: GET / ===" -ForegroundColor Green
& $curl -v http://localhost:8080/

Write-Host "`n=== Test 2: GET /stream (5s) ===" -ForegroundColor Green
& $curl -v -N --max-time 5 http://localhost:8080/stream

# Write-Host "`n=== Test 3: 404 ===" -ForegroundColor Green
# & $curl -v http://localhost:8080/noexiste

# Write-Host "`n=== Test 4: Headers only ===" -ForegroundColor Green
# & $curl -I http://localhost:8080/
# & $curl -I http://localhost:8080/stream