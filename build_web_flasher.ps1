# PowerShell script to build and prepare web flasher files
# Run this after building the firmware with PlatformIO

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "  Poker Timer CYD - Web Flasher Builder" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# Check if build directory exists
if (-not (Test-Path ".pio\build\esp32-2432s028")) {
    Write-Host "Error: Build directory not found!" -ForegroundColor Red
    Write-Host "Please build the firmware first using PlatformIO:" -ForegroundColor Yellow
    Write-Host "  pio run --environment esp32-2432s028" -ForegroundColor Yellow
    Write-Host "Or use the PlatformIO Build button in VS Code" -ForegroundColor Yellow
    exit 1
}

# Create web_flasher directory if it doesn't exist
if (-not (Test-Path "web_flasher")) {
    Write-Host "Creating web_flasher directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "web_flasher" | Out-Null
}

Write-Host "Copying firmware binaries..." -ForegroundColor Green

# Copy firmware binary
if (Test-Path ".pio\build\esp32-2432s028\firmware.bin") {
    Copy-Item ".pio\build\esp32-2432s028\firmware.bin" "web_flasher\firmware.bin"
    $size = (Get-Item "web_flasher\firmware.bin").Length
    Write-Host "  ✓ firmware.bin ($([math]::Round($size/1KB, 2)) KB)" -ForegroundColor White
} else {
    Write-Host "  ✗ firmware.bin not found!" -ForegroundColor Red
}

# Copy bootloader
if (Test-Path ".pio\build\esp32-2432s028\bootloader.bin") {
    Copy-Item ".pio\build\esp32-2432s028\bootloader.bin" "web_flasher\bootloader.bin"
    $size = (Get-Item "web_flasher\bootloader.bin").Length
    Write-Host "  ✓ bootloader.bin ($([math]::Round($size/1KB, 2)) KB)" -ForegroundColor White
} else {
    Write-Host "  ✗ bootloader.bin not found!" -ForegroundColor Red
}

# Copy partitions
if (Test-Path ".pio\build\esp32-2432s028\partitions.bin") {
    Copy-Item ".pio\build\esp32-2432s028\partitions.bin" "web_flasher\partitions.bin"
    $size = (Get-Item "web_flasher\partitions.bin").Length
    Write-Host "  ✓ partitions.bin ($([math]::Round($size/1KB, 2)) KB)" -ForegroundColor White
} else {
    Write-Host "  ✗ partitions.bin not found!" -ForegroundColor Red
}

# Look for boot_app0.bin in common locations
$bootAppLocations = @(
    "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin",
    ".pio\build\esp32-2432s028\boot_app0.bin"
)

$bootAppFound = $false
foreach ($location in $bootAppLocations) {
    if (Test-Path $location) {
        Copy-Item $location "web_flasher\boot_app0.bin"
        $bootAppFound = $true
        $size = (Get-Item "web_flasher\boot_app0.bin").Length
        Write-Host "  ✓ boot_app0.bin ($([math]::Round($size/1KB, 2)) KB)" -ForegroundColor White
        break
    }
}

if (-not $bootAppFound) {
    Write-Host "  ! boot_app0.bin not found, creating..." -ForegroundColor Yellow
    # Create a blank boot_app0.bin (4KB filled with 0xFF)
    $bootApp = New-Object byte[] 4096
    for ($i = 0; $i -lt 4096; $i++) { $bootApp[$i] = 0xFF }
    [System.IO.File]::WriteAllBytes("$PWD\web_flasher\boot_app0.bin", $bootApp)
    Write-Host "  ✓ boot_app0.bin created (4.00 KB)" -ForegroundColor White
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Build complete! Files ready in web_flasher/" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "To test locally:" -ForegroundColor Yellow
Write-Host "  1. cd web_flasher" -ForegroundColor White
Write-Host "  2. python -m http.server 8000" -ForegroundColor White
Write-Host "  3. Open http://localhost:8000 in Chrome/Edge" -ForegroundColor White
Write-Host ""
Write-Host "Files in web_flasher directory:" -ForegroundColor Cyan
Get-ChildItem "web_flasher" -File | ForEach-Object {
    $size = [math]::Round($_.Length / 1KB, 2)
    Write-Host ("  {0,-20} {1,10} KB" -f $_.Name, $size) -ForegroundColor White
}
Write-Host ""
