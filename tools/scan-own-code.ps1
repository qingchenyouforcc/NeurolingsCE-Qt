$ErrorActionPreference = "Stop"

$Root = Resolve-Path "$PSScriptRoot\.."
$ScanDir = Join-Path $env:TEMP "NeurolingsCE-fuc-scan"

Write-Host "[1/4] Root: $Root"
Write-Host "[2/4] Temp scan dir: $ScanDir"

Remove-Item $ScanDir -Recurse -Force -ErrorAction SilentlyContinue

$ExcludeDirs = @(
    ".git",
    ".vs",
    ".idea",
    ".vscode",
    "build",
    "out",
    "dist",
    "release",
    "debug",
    "cmake-build-debug",
    "cmake-build-release",
    "cpp-httplib",
    "libshimejifinder",
    "ElaWidgetTools",
    "rapidxml",
    "duktape"
)

$ExcludeFiles = @(
    "*.png", "*.jpg", "*.jpeg", "*.gif", "*.ico",
    "*.zip", "*.7z", "*.rar",
    "*.dll", "*.exe", "*.pdb", "*.obj", "*.lib", "*.a", "*.so", "*.dylib",
    "*.user", "*.suo"
)

Write-Host "[3/4] Copying own code..."

robocopy $Root $ScanDir /E /MT:8 /R:1 /W:1 /XD $ExcludeDirs /XF $ExcludeFiles /NFL /NDL /NP

$RoboExit = $LASTEXITCODE

if ($RoboExit -ge 8) {
    Write-Host "robocopy failed with exit code $RoboExit"
    exit $RoboExit
}

Write-Host "[4/4] Running fuck-u-code analyze..."
Push-Location $ScanDir
fuck-u-code analyze
Pop-Location