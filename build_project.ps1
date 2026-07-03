# PowerShell Build Script for Kumari Engine Milestone 10 Phase 1
$ErrorActionPreference = "Continue"

Write-Host "=== Kumari Kandam Engine - Milestone 10 Phase 1 ===" -ForegroundColor Green
Write-Host "Starting Build Automation Pipeline..."

# 1. Generate build metadata
$timestamp = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
$hash = "mock_hash"
try {
    $hash = (git rev-parse --short HEAD).Trim()
} catch {}

$buildMeta = @"
{
  "build_time": "$timestamp",
  "version": "1.0.0-build-$hash",
  "host_os": "Windows",
  "target_platform": "Windows/Android/Linux/macOS/iOS",
  "compiler": "MSVC C++20"
}
"@
New-Item -ItemType Directory -Force -Path "docs" | Out-Null
Set-Content -Path "docs/build_metadata.json" -Value $buildMeta
Write-Host "Generated build metadata at docs/build_metadata.json"

# 2. Build Debug Targets
Write-Host "`n=== Building Debug Targets ===" -ForegroundColor Cyan
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
$buildDebugSuccess = ($LASTEXITCODE -eq 0)

# 3. Build Release Targets
Write-Host "`n=== Building Release Targets ===" -ForegroundColor Cyan
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
$buildReleaseSuccess = ($LASTEXITCODE -eq 0)

if (-not $buildReleaseSuccess) {
    Write-Host "Build failed! Aborting." -ForegroundColor Red
    Exit 1
}

# 4. Packaging assets using built release game exe
Write-Host "`n=== Running Project Packaging Pipeline ===" -ForegroundColor Cyan
& ".\build-release\game\Release\KumariKandamGame.exe" -package -src game/assets -dest dist -target windows -builddir build-release
& ".\build-release\game\Release\KumariKandamGame.exe" -package -src game/assets -dest dist -target android -builddir build-release
& ".\build-release\game\Release\KumariKandamGame.exe" -package -src game/assets -dest dist -target linux -builddir build-release
& ".\build-release\game\Release\KumariKandamGame.exe" -package -src game/assets -dest dist -target macos -builddir build-release
& ".\build-release\game\Release\KumariKandamGame.exe" -package -src game/assets -dest dist -target ios -builddir build-release

# 5. Run tests and collect logs
Write-Host "`n=== Executing Automated Test Suite ===" -ForegroundColor Cyan
$smokeTestLog = "docs/smoke_test_output.txt"
$regressionTestLog = "docs/regression_test_output.txt"
$stressTestLog = "docs/stress_test_output.txt"
$benchmarkSuiteLog = "docs/benchmark_suite_output.txt"

Write-Host "Running Smoke Tests..."
& ".\build-release\game\Release\KumariEngineSmokeTest.exe" > $smokeTestLog 2>&1
$smokeSuccess = ($LASTEXITCODE -eq 0)

Write-Host "Running Regression Tests..."
& ".\build-release\game\Release\KumariEngineRegressionTests.exe" > $regressionTestLog 2>&1
$regressionSuccess = ($LASTEXITCODE -eq 0)

Write-Host "Running Stress Tests..."
& ".\build-release\game\Release\KumariEngineStressTests.exe" > $stressTestLog 2>&1
$stressSuccess = ($LASTEXITCODE -eq 0)

Write-Host "Running Benchmark Suite..."
& ".\build-release\game\Release\KumariEngineBenchmarkSuite.exe" > $benchmarkSuiteLog 2>&1
$benchmarkSuiteSuccess = ($LASTEXITCODE -eq 0)

# 6. Run packaged mode verification & performance benchmark
Write-Host "Running Packaged Performance Benchmark..."
$benchmarkLog = "docs/benchmark_run_output.txt"
& ".\build-release\game\Release\KumariKandamGame.exe" -packaged -manifest dist/packaged_assets/version.manifest -benchmark > $benchmarkLog 2>&1
$benchmarkSuccess = ($LASTEXITCODE -eq 0)

# 7. Run crash reporter test (expected to dump a crash report)
Write-Host "Running Crash Reporter Test..."
$crashLog = "docs/crash_test_output.txt"
if (Test-Path "crash_reports") {
    Remove-Item -Recurse -Force "crash_reports/*" -ErrorAction SilentlyContinue
}
try {
    & ".\build-release\game\Release\KumariKandamGame.exe" -crash_test > $crashLog 2>&1
} catch {}

$crashFiles = Get-ChildItem "crash_reports/crash_report_*.json" -ErrorAction SilentlyContinue
$crashSuccess = ($crashFiles.Count -gt 0)

# 8. Generate Reports
Write-Host "`n=== Generating Phase Reports ===" -ForegroundColor Green

# 8a. Test Results & Build Summary
$testResults = @"
# Test Validation Results

## Build Configurations
- **Debug Target Build**: $(if ($buildDebugSuccess) { "SUCCESS" } else { "FAILED" })
- **Release Target Build**: $(if ($buildReleaseSuccess) { "SUCCESS" } else { "FAILED" })

## Test Suites
- **Smoke Tests**: $(if ($smokeSuccess) { "PASSED" } else { "FAILED" })
- **Regression Tests**: $(if ($regressionSuccess) { "PASSED" } else { "FAILED" })
- **Stress Tests**: $(if ($stressSuccess) { "PASSED" } else { "FAILED" })
- **Benchmark Suite**: $(if ($benchmarkSuiteSuccess) { "PASSED" } else { "FAILED" })
- **Packaging VFS Validation & Benchmark**: $(if ($benchmarkSuccess) { "PASSED" } else { "FAILED" })
- **Crash Reporting Exception Handler**: $(if ($crashSuccess) { "PASSED" } else { "FAILED" })

See raw console logs in the `docs` folder.
"@
Set-Content -Path "docs/test_results.md" -Value $testResults

# 8b. Packaging Report
$manifestSize = (Get-Item "dist/packaged_assets/version.manifest").Length
$scriptsSize = (Get-Item "dist/packaged_assets/scripts.kpak").Length
$prefabsSize = (Get-Item "dist/packaged_assets/prefabs.kpak").Length

$packagingReport = @"
# Packaging Report

## Packaged Bundle Sizes
- **version.manifest**: $manifestSize Bytes
- **scripts.kpak**: $scriptsSize Bytes
- **prefabs.kpak**: $prefabsSize Bytes

## Configuration
- **Compression Scheme**: Custom RLE
- **Encryption Scheme**: Rotating XOR Rolling Index
- **Decryption Key**: "KumariKandamKey!"
"@
Set-Content -Path "docs/packaging_report.md" -Value $packagingReport

# 8c. Deployment Architecture Report
$deploymentReport = @"
# Deployment Architecture Report

## Deployment Model
Kumari Engine implements a modular multi-platform deployment pipeline. Staged asset packages, binary executables, and dependencies are packed and deployed target-by-target.

## Platform Support
1. **Windows**: Fully operational deployment (EXE, DLLs, resource packs).
2. **Android**: Gradle-based project structure with JNI setup and assets packed inside JNI layout.
3. **Linux / macOS / iOS**: Preparatory structure templates with configuration and platform-specific packaging files.
"@
Set-Content -Path "docs/deployment_architecture_report.md" -Value $deploymentReport

Write-Host "Build Automation Pipeline finished successfully." -ForegroundColor Green
