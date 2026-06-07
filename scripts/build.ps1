$ErrorActionPreference = "Stop"
$Preset = if ($args.Count -gt 0) { $args[0] } else { "vs2022-release" }

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock] $Command
    )

    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

function Remove-StaleCMakeCache {
    param(
        [Parameter(Mandatory = $true)]
        [string] $BuildDir
    )

    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath)) {
        return
    }

    $cacheLine = Select-String -LiteralPath $cachePath -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$' | Select-Object -First 1
    if ($null -eq $cacheLine) {
        return
    }

    $workspace = (Resolve-Path ".").Path.TrimEnd('\', '/')
    $cachedSource = [System.IO.Path]::GetFullPath($cacheLine.Matches[0].Groups[1].Value).TrimEnd('\', '/')
    if ([string]::Equals($workspace, $cachedSource, [System.StringComparison]::OrdinalIgnoreCase)) {
        return
    }

    $resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path.TrimEnd('\', '/')
    $workspacePrefix = $workspace + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedBuildDir.StartsWith($workspacePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove stale CMake cache outside workspace: $resolvedBuildDir"
    }

    Write-Host "Removing stale CMake build directory '$resolvedBuildDir' created for '$cachedSource'."
    Remove-Item -LiteralPath $resolvedBuildDir -Recurse -Force
}

switch ($Preset) {
    "vs2022-release" {
        Remove-StaleCMakeCache "build\vs2022"
        Invoke-Checked { cmake --preset vs2022 }
        Invoke-Checked { cmake --build --preset vs2022-release }
        Invoke-Checked { ctest --preset vs2022-release }
    }
    "mingw-release" {
        Remove-StaleCMakeCache "build\mingw-release"
        Invoke-Checked { cmake --preset mingw-release }
        Invoke-Checked { cmake --build --preset mingw-release }
        Invoke-Checked { ctest --preset mingw-release }
    }
    "mingw-debug" {
        Remove-StaleCMakeCache "build\mingw-debug"
        Invoke-Checked { cmake --preset mingw-debug }
        Invoke-Checked { cmake --build --preset mingw-debug }
        Invoke-Checked { ctest --preset mingw-debug }
    }
    default {
        throw "Unknown build preset '$Preset'. Use vs2022-release, mingw-release, or mingw-debug."
    }
}
