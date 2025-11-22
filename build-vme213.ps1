# Build script for Heltec VisionMaster E213
$ErrorActionPreference = "Stop"

$ARDUINO_CLI = ".\tools\arduino-cli.exe"
$BOARD_MODEL = "0x40"
$ARDUINO_ESP_CORE_VER = "2.0.17"

Write-Host "=== RNode Firmware Build for VME213 ===" -ForegroundColor Cyan

# Check if arduino-cli exists
if (-not (Test-Path $ARDUINO_CLI)) {
    Write-Host "ERROR: arduino-cli not found at $ARDUINO_CLI" -ForegroundColor Red
    exit 1
}

# Update core index
Write-Host "`nUpdating Arduino core index..." -ForegroundColor Yellow
& $ARDUINO_CLI core update-index --config-file arduino-cli.yaml

# Install ESP32 core if not installed
Write-Host "`nChecking ESP32 core..." -ForegroundColor Yellow
$coreInstalled = & $ARDUINO_CLI core list | Select-String "esp32:esp32"
if (-not $coreInstalled) {
    Write-Host "Installing ESP32 core $ARDUINO_ESP_CORE_VER..." -ForegroundColor Yellow
    & $ARDUINO_CLI core install "esp32:esp32@$ARDUINO_ESP_CORE_VER" --config-file arduino-cli.yaml
}

# Install required libraries
Write-Host "`nChecking required libraries..." -ForegroundColor Yellow
$requiredLibs = @(
    "Adafruit SSD1306",
    "Adafruit SH110X",
    "Adafruit GFX Library",
    "Adafruit BusIO",
    "Crypto"
)

foreach ($lib in $requiredLibs) {
    $libInstalled = & $ARDUINO_CLI lib list | Select-String $lib
    if (-not $libInstalled) {
        Write-Host "Installing $lib..." -ForegroundColor Yellow
        & $ARDUINO_CLI lib install $lib
    }
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Compile firmware
Write-Host "`nCompiling firmware for VME213..." -ForegroundColor Green
& $ARDUINO_CLI compile `
    --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=no_ota `
    --build-property "build.extra_flags=-DBOARD_MODEL=$BOARD_MODEL" `
    --output-dir build `
    --verbose `
    RNode_Firmware.ino

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== Build SUCCESS ===" -ForegroundColor Green
    Write-Host "Firmware binary: build\RNode_Firmware.ino.bin" -ForegroundColor Cyan
    
    # Show file size
    $binFile = "build\RNode_Firmware.ino.bin"
    if (Test-Path $binFile) {
        $size = (Get-Item $binFile).Length
        $sizeKB = [math]::Round($size / 1KB, 2)
        Write-Host "Binary size: $sizeKB KB" -ForegroundColor Cyan
    }
} else {
    Write-Host "`n=== Build FAILED ===" -ForegroundColor Red
    exit 1
}
