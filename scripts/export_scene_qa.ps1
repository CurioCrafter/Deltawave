param(
    [string]$OutputRoot = "",
    [int]$Width = 960,
    [int]$Height = 540,
    [int]$Fps = 4,
    [double]$Seconds = 3.0,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputRoot = Join-Path $repoRoot "artifacts\scene_qa_$stamp"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot $OutputRoot
}

$exporter = Join-Path $repoRoot "build\vs2022\Release\VisualizerExport.exe"
if (-not (Test-Path -LiteralPath $exporter)) {
    if ($SkipBuild) {
        throw "VisualizerExport.exe was not found at $exporter."
    }
    & (Join-Path $repoRoot "scripts\build.ps1")
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$audioRoot = Join-Path $OutputRoot "audio"
New-Item -ItemType Directory -Force -Path $audioRoot | Out-Null

function Write-Ascii([System.IO.BinaryWriter]$Writer, [string]$Text) {
    $Writer.Write([System.Text.Encoding]::ASCII.GetBytes($Text))
}

function Write-WavProfile {
    param(
        [string]$Path,
        [scriptblock]$Generator,
        [double]$DurationSeconds = 3.0,
        [int]$SampleRate = 44100
    )

    $channels = 2
    $bitsPerSample = 16
    $bytesPerSample = [int]($bitsPerSample / 8)
    $sampleCount = [int]([Math]::Round($DurationSeconds * $SampleRate))
    $dataSize = $sampleCount * $channels * $bytesPerSample

    $stream = [System.IO.File]::Create($Path)
    try {
        $writer = [System.IO.BinaryWriter]::new($stream)
        try {
            Write-Ascii $writer "RIFF"
            $writer.Write([int](36 + $dataSize))
            Write-Ascii $writer "WAVE"
            Write-Ascii $writer "fmt "
            $writer.Write([int]16)
            $writer.Write([int16]1)
            $writer.Write([int16]$channels)
            $writer.Write([int]$SampleRate)
            $writer.Write([int]($SampleRate * $channels * $bytesPerSample))
            $writer.Write([int16]($channels * $bytesPerSample))
            $writer.Write([int16]$bitsPerSample)
            Write-Ascii $writer "data"
            $writer.Write([int]$dataSize)

            for ($i = 0; $i -lt $sampleCount; ++$i) {
                $t = [double]$i / [double]$SampleRate
                $value = & $Generator $t $i
                if ($null -eq $value) {
                    $left = 0.0
                    $right = 0.0
                } elseif ($value.Count -ge 2) {
                    $left = [double]$value[0]
                    $right = [double]$value[1]
                } else {
                    $left = [double]$value
                    $right = $left
                }
                $left = [Math]::Max(-1.0, [Math]::Min(1.0, $left))
                $right = [Math]::Max(-1.0, [Math]::Min(1.0, $right))
                $writer.Write([int16][Math]::Round($left * 32767.0))
                $writer.Write([int16][Math]::Round($right * 32767.0))
            }
        } finally {
            if ($writer) {
                $writer.Dispose()
            }
        }
    } finally {
        $stream.Dispose()
    }
}

function Pulse([double]$T, [double]$Rate, [double]$Sharpness) {
    $phase = ($T * $Rate) % 1.0
    return [double]([Math]::Exp(-$phase * $Sharpness))
}

function Sine([double]$T, [double]$Hz) {
    return [double]([Math]::Sin(2.0 * [Math]::PI * $Hz * $T))
}

function Stereo([double]$Left, [double]$Right) {
    return @($Left, $Right)
}

$profiles = @(
    @{
        Name = "silence"
        Generator = {
            param($t, $i)
            Stereo 0.0 0.0
        }
    },
    @{
        Name = "low_volume"
        Generator = {
            param($t, $i)
            $pad = 0.020 * (Sine $t 146.83) + 0.014 * (Sine $t 220.00) + 0.008 * (Sine $t 329.63)
            Stereo ($pad * 0.85) ($pad * 1.05)
        }
    },
    @{
        Name = "techno"
        Generator = {
            param($t, $i)
            $kick = (Pulse $t 2.25 24.0) * (Sine $t 62.0) * 0.24
            $bass = (Sine $t 124.0) * (0.070 + 0.035 * (Pulse $t 4.5 10.0))
            $hat = (Sine $t 2460.0) * (Pulse ($t + 0.055) 9.0 20.0) * 0.18
            $seq = ((Sine $t 330.0) + 0.55 * (Sine $t 495.0)) * (Pulse ($t + 0.02) 4.5 8.0) * 0.24
            Stereo ($kick + $bass + $hat + $seq) ($kick + $bass * 0.94 - $hat * 0.72 + $seq * 0.88)
        }
    },
    @{
        Name = "bass_drop"
        Generator = {
            param($t, $i)
            $drop = if ($t -gt 0.75) { [Math]::Min(1.0, ($t - 0.75) / 0.45) } else { 0.0 }
            $pulse = Pulse $t 1.5 16.0
            $sub = (Sine $t 39.0) * (0.25 + $drop * 0.46) * (0.72 + $pulse * 0.28)
            $growl = (Sine $t 78.0) * $drop * 0.22
            $impact = (Pulse ($t - 0.75) 1.5 36.0) * $drop * (Sine $t 58.0) * 0.38
            Stereo ($sub + $growl + $impact) ($sub * 0.96 + $growl * 1.08 + $impact)
        }
    },
    @{
        Name = "ambient"
        Generator = {
            param($t, $i)
            $drift = 0.5 + 0.5 * (Sine $t 0.11)
            $left = 0.095 * (Sine $t 220.0) + 0.080 * (Sine $t 277.18) + 0.066 * (Sine $t 329.63)
            $right = 0.090 * (Sine $t 222.7) + 0.084 * (Sine $t 279.4) + 0.064 * (Sine $t 331.9)
            Stereo ($left * (0.68 + $drift * 0.18)) ($right * (0.72 + (1.0 - $drift) * 0.16))
        }
    },
    @{
        Name = "melodic"
        Generator = {
            param($t, $i)
            $notes = @(440.00, 554.37, 659.25, 830.61, 880.00, 1108.73)
            $step = [int][Math]::Floor($t * 5.0) % $notes.Count
            $env = Pulse $t 5.0 5.5
            $lead = (Sine $t $notes[$step]) * (0.12 + $env * 0.25)
            $harm = (Sine $t ($notes[($step + 2) % $notes.Count] * 0.5)) * 0.085
            $spark = (Sine $t ($notes[$step] * 2.0)) * $env * 0.095
            Stereo ($lead + $harm + $spark) ($lead * 0.82 + $harm * 1.08 - $spark * 0.42)
        }
    },
    @{
        Name = "breakbeat"
        Generator = {
            param($t, $i)
            $bar = ($t * 2.05) % 1.0
            $hit = 0.0
            foreach ($p in @(0.00, 0.17, 0.39, 0.58, 0.77, 0.91)) {
                $d = [Math]::Abs($bar - $p)
                $d = [Math]::Min($d, 1.0 - $d)
                $hit += [Math]::Exp(-$d * 58.0)
            }
            $gate = [Math]::Min(1.0, $hit * 1.7)
            $kick = (Sine $t 72.0) * [Math]::Min(1.0, $hit) * 0.18
            $cut = ((Sine $t 2600.0) + 0.60 * (Sine $t 5200.0) + 0.28 * (Sine $t 7800.0)) * $gate * 0.32
            $bass = (Sine $t 132.0) * 0.055
            Stereo ($kick + $cut + $bass) ($kick * 0.82 - $cut * 0.74 + $bass)
        }
    },
    @{
        Name = "dark_minimal"
        Generator = {
            param($t, $i)
            $thump = (Pulse $t 1.15 24.0) * (Sine $t 48.0) * 0.36
            $drone = (Sine $t 72.0) * 0.20 + (Sine $t 108.0) * 0.08
            $minor = (Sine $t 155.56) * 0.045
            Stereo ($thump + $drone + $minor) ($thump * 0.98 + $drone * 0.92 - $minor * 0.6)
        }
    }
)

$summaryRows = @()
foreach ($profile in $profiles) {
    $name = $profile.Name
    $wavPath = Join-Path $audioRoot "$name.wav"
    Write-Host "Writing $name audio..."
    Write-WavProfile -Path $wavPath -Generator $profile.Generator -DurationSeconds $Seconds

    $profileOutput = Join-Path $OutputRoot $name
    Write-Host "Exporting $name..."
    & $exporter --input $wavPath --output $profileOutput --width $Width --height $Height --fps $Fps --seconds $Seconds --auto-scene --depth-3d 1.0 --object-density-3d 0.86 --lighting-glow 0.82 --color-impact 0.96 --scene-personality 0.88 --response-3d 0.96 --motion-stability 0.92 --pattern-clarity 0.96 --complexity 1.05 --share --no-trails
    if ($LASTEXITCODE -ne 0) {
        throw "VisualizerExport failed for $name with exit code $LASTEXITCODE."
    }

    $timelinePath = Join-Path $profileOutput "analysis_timeline.csv"
    $timeline = Import-Csv -Path $timelinePath
    if ($timeline.Count -le 0) {
        throw "No timeline rows written for $name."
    }

    $metric = {
        param($Rows, $Column, $Mode)
        $values = @($Rows | ForEach-Object { [double]$_.$Column })
        if ($Mode -eq "min") {
            return ($values | Measure-Object -Minimum).Minimum
        }
        return ($values | Measure-Object -Maximum).Maximum
    }
    $last = $timeline[$timeline.Count - 1]
    $columnMax = {
        param($Rows, $Column)
        if ($Rows[0].PSObject.Properties.Name -contains $Column) {
            return & $metric $Rows $Column "max"
        }
        return 0.0
    }

    $row = [pscustomobject]@{
        profile = $name
        rows = $timeline.Count
        finalMode = $last.mode
        finalMotion = $last.motionStyle
        finalStyle = $last.style
        retained2DMax = & $metric $timeline "retained2DPrimitiveCount" "max"
        coverageMin = & $metric $timeline "projected3DScreenCoverage" "min"
        centerOffsetMax = & $metric $timeline "projected3DCenterOffset" "max"
        materialShareMin = & $metric $timeline "projected3DMaterialShare" "min"
        foregroundMin = & $metric $timeline "foreground3DShare" "min"
        midgroundMin = & $metric $timeline "midground3DShare" "min"
        backgroundMin = & $metric $timeline "background3DShare" "min"
        cameraMotionMax = & $columnMax $timeline "cameraMotion3D"
        cameraContinuityMin = if ($timeline[0].PSObject.Properties.Name -contains "cameraContinuity3D") { & $metric $timeline "cameraContinuity3D" "min" } else { 0.0 }
        cameraContinuityMax = & $columnMax $timeline "cameraContinuity3D"
        explicitRoleShareMax = if ($timeline[0].PSObject.Properties.Name -contains "explicitRoleShare3D") { & $metric $timeline "explicitRoleShare3D" "max" } else { 0.0 }
        bridgeShareMax = if ($timeline[0].PSObject.Properties.Name -contains "roleBridgeShare3D") { & $metric $timeline "roleBridgeShare3D" "max" } else { 0.0 }
        roleCrosstalkMax = if ($timeline[0].PSObject.Properties.Name -contains "roleCrosstalk3D") { & $metric $timeline "roleCrosstalk3D" "max" } else { 1.0 }
        districtSpreadMax = if ($timeline[0].PSObject.Properties.Name -contains "roleDistrictSpread3D") { & $metric $timeline "roleDistrictSpread3D" "max" } else { 0.0 }
        roleVocabularyMax = if ($timeline[0].PSObject.Properties.Name -contains "roleVocabulary3D") { & $metric $timeline "roleVocabulary3D" "max" } else { 0.0 }
        roleSilhouetteContrastMax = if ($timeline[0].PSObject.Properties.Name -contains "roleSilhouetteContrast3D") { & $metric $timeline "roleSilhouetteContrast3D" "max" } else { 0.0 }
        roleLegibilityMax = if ($timeline[0].PSObject.Properties.Name -contains "roleLegibility3D") { & $metric $timeline "roleLegibility3D" "max" } else { 0.0 }
        bassRoleMax = & $columnMax $timeline "bassRole3D"
        drumRoleMax = & $columnMax $timeline "drumRole3D"
        melodyRoleMax = & $columnMax $timeline "melodyRole3D"
        harmonyRoleMax = & $columnMax $timeline "harmonyRole3D"
        spaceRoleMax = & $columnMax $timeline "spaceRole3D"
        fractureRoleMax = & $columnMax $timeline "fractureRole3D"
        shadowRoleMax = & $columnMax $timeline "shadowRole3D"
        convergenceRoleMax = & $columnMax $timeline "convergence3D"
        songArcMax = & $columnMax $timeline "songArc3D"
        anticipationArcMax = & $columnMax $timeline "songArcAnticipation3D"
        impactArcMax = & $columnMax $timeline "songArcImpact3D"
        recoveryArcMax = & $columnMax $timeline "songArcRecovery3D"
        continuityArcMax = & $columnMax $timeline "songArcContinuity3D"
        preview = Join-Path $profileOutput "preview.bmp"
        timeline = $timelinePath
    }
    $summaryRows += $row
}

$summaryPath = Join-Path $OutputRoot "scene_qa_summary.csv"
$summaryRows | Export-Csv -Path $summaryPath -NoTypeInformation

$failures = @()
foreach ($row in $summaryRows) {
    if ([double]$row.retained2DMax -gt 0.0) {
        $failures += "$($row.profile): retained 2D primitives were present"
    }
    if ([double]$row.coverageMin -lt 0.055) {
        $failures += "$($row.profile): projected 3D coverage below threshold"
    }
    if ([double]$row.materialShareMin -lt 0.24) {
        $failures += "$($row.profile): material surface share below threshold"
    }
    if ([double]$row.centerOffsetMax -gt 0.62) {
        $failures += "$($row.profile): projected 3D scene drifted too far off center"
    }
    if ($row.profile -notin @("silence", "low_volume")) {
        if ([double]$row.explicitRoleShareMax -lt 0.38) {
            $failures += "$($row.profile): explicit music-role geometry share below threshold"
        }
        if ([double]$row.districtSpreadMax -lt 0.72) {
            $failures += "$($row.profile): role districts did not spread enough across 3D space"
        }
        if ([double]$row.roleVocabularyMax -lt 0.28) {
            $failures += "$($row.profile): role geometry vocabulary was not distinct enough"
        }
        if ([double]$row.roleSilhouetteContrastMax -lt 0.09) {
            $failures += "$($row.profile): role silhouettes/materials were not distinct enough"
        }
        if ([double]$row.roleLegibilityMax -lt 0.28) {
            $failures += "$($row.profile): role legibility stayed too low; separate music parts are likely meshing together"
        }
    }
    switch ($row.profile) {
        "techno" {
            if ($row.finalMode -notin @("Techno Mandala", "Polyrhythm Lattice") -or $row.finalMotion -ne "Mechanical") {
                $failures += "techno: final scene should stay mechanical rhythm architecture, got $($row.finalMode) / $($row.finalMotion)"
            }
            if ([double]$row.drumRoleMax -lt 0.22) {
                $failures += "techno: drum/sequencer role did not dominate enough"
            }
        }
        "bass_drop" {
            if ($row.finalMode -ne "Quantum Tunnel" -or $row.finalMotion -ne "Heavy Bass") {
                $failures += "bass_drop: final scene should stay Quantum Tunnel / Heavy Bass, got $($row.finalMode) / $($row.finalMotion)"
            }
            if ([double]$row.bassRoleMax -lt 0.34 -or [double]$row.convergenceRoleMax -lt 0.10) {
                $failures += "bass_drop: bass pressure/convergence role did not respond enough"
            }
            if ([double]$row.impactArcMax -lt 0.24) {
                $failures += "bass_drop: persistent song-arc impact did not rise enough"
            }
        }
        "ambient" {
            if ([double]$row.spaceRoleMax -lt 0.30) {
                $failures += "ambient: spatial depth role did not dominate enough"
            }
        }
        "melodic" {
            if (([double]$row.melodyRoleMax + [double]$row.harmonyRoleMax) -lt 0.34) {
                $failures += "melodic: melody/harmony roles did not dominate enough"
            }
            if ([double]$row.recoveryArcMax -lt 0.04 -and [double]$row.anticipationArcMax -lt 0.05) {
                $failures += "melodic: song-arc memory did not expose phrase lift or recovery"
            }
        }
        "breakbeat" {
            if ($row.finalMode -ne "Spectral Origami" -or $row.finalMotion -ne "Breakbeat") {
                $failures += "breakbeat: final scene should be Spectral Origami / Breakbeat, got $($row.finalMode) / $($row.finalMotion)"
            }
            if ([double]$row.fractureRoleMax -lt 0.28) {
                $failures += "breakbeat: fracture/cut-plane role did not dominate enough"
            }
        }
        "dark_minimal" {
            if ([double]$row.shadowRoleMax -lt 0.24) {
                $failures += "dark_minimal: shadow/monolith role did not dominate enough"
            }
        }
    }
}

$contactSheetPath = Join-Path $OutputRoot "scene_contact_sheet.bmp"
$contactSheetWritten = $false
try {
    Add-Type -AssemblyName System.Drawing
    $images = @()
    foreach ($row in $summaryRows) {
        $images += [pscustomobject]@{
            Row = $row
            Image = [System.Drawing.Bitmap]::FromFile($row.preview)
        }
    }

    $columns = 2
    $labelHeight = 48
    $cellWidth = [int](($images | ForEach-Object { $_.Image.Width } | Measure-Object -Maximum).Maximum)
    $imageHeight = [int](($images | ForEach-Object { $_.Image.Height } | Measure-Object -Maximum).Maximum)
    $cellHeight = [int]($imageHeight + $labelHeight)
    $rows = [int][Math]::Ceiling($images.Count / [double]$columns)
    $sheetWidth = [int]($cellWidth * $columns)
    $sheetHeight = [int]($cellHeight * $rows)
    $sheet = [System.Drawing.Bitmap]::new($sheetWidth, $sheetHeight)
    $graphics = [System.Drawing.Graphics]::FromImage($sheet)
    $graphics.Clear([System.Drawing.Color]::FromArgb(18, 20, 24))
    $font = [System.Drawing.Font]::new("Segoe UI", 10.0)
    $brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
    $mutedBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(190, 210, 220))
    for ($i = 0; $i -lt $images.Count; ++$i) {
        $x = ($i % $columns) * $cellWidth
        $y = [int][Math]::Floor($i / $columns) * $cellHeight
        $row = $images[$i].Row
        $graphics.DrawString($row.profile, $font, $brush, [single]($x + 8), [single]($y + 5))
        $metrics = "$($row.finalMode) / $($row.finalMotion)  cov $([Math]::Round([double]$row.coverageMin, 3))  spread $([Math]::Round([double]$row.districtSpreadMax, 3))  read $([Math]::Round([double]$row.roleLegibilityMax, 3))"
        $graphics.DrawString($metrics, $font, $mutedBrush, [single]($x + 8), [single]($y + 24))
        $graphics.DrawImage($images[$i].Image, $x, $y + $labelHeight, $images[$i].Image.Width, $images[$i].Image.Height)
    }
    $sheet.Save($contactSheetPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $contactSheetWritten = $true
} finally {
    if ($graphics) { $graphics.Dispose() }
    if ($sheet) { $sheet.Dispose() }
    if ($font) { $font.Dispose() }
    if ($brush) { $brush.Dispose() }
    if ($mutedBrush) { $mutedBrush.Dispose() }
    if ($images) {
        foreach ($item in $images) {
            if ($item.Image) {
                $item.Image.Dispose()
            }
        }
    }
}

$htmlPath = Join-Path $OutputRoot "index.html"
$html = New-Object System.Text.StringBuilder
[void]$html.AppendLine("<!doctype html><meta charset='utf-8'><title>Deltawave 3D Scene QA</title>")
[void]$html.AppendLine("<style>body{font-family:Segoe UI,Arial,sans-serif;background:#101418;color:#eef;margin:24px}table{border-collapse:collapse;width:100%;margin-top:16px}td,th{border:1px solid #334;padding:6px;text-align:left}img{max-width:100%;border:1px solid #334;background:#222}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}.card{background:#171d24;padding:12px}</style>")
[void]$html.AppendLine("<h1>Deltawave 3D Scene QA</h1>")
[void]$html.AppendLine("<p>Generated $(Get-Date -Format u). Each profile is rendered through VisualizerExport with auto-scene and 3D-first settings.</p>")
if ($contactSheetWritten) {
    [void]$html.AppendLine("<p><a href='scene_contact_sheet.bmp'>Combined BMP contact sheet</a></p>")
}
[void]$html.AppendLine("<div class='grid'>")
foreach ($row in $summaryRows) {
    $relativePreview = "$($row.profile)/preview.bmp"
    [void]$html.AppendLine("<div class='card'><h2>$($row.profile)</h2><a href='$relativePreview'><img src='$relativePreview'></a><p>$($row.finalMode) / $($row.finalMotion) / $($row.finalStyle). coverage min $([Math]::Round([double]$row.coverageMin, 3)), material min $([Math]::Round([double]$row.materialShareMin, 3)), role spread max $([Math]::Round([double]$row.districtSpreadMax, 3)), vocabulary max $([Math]::Round([double]$row.roleVocabularyMax, 3)), silhouette max $([Math]::Round([double]$row.roleSilhouetteContrastMax, 3)), legibility max $([Math]::Round([double]$row.roleLegibilityMax, 3)), crosstalk max $([Math]::Round([double]$row.roleCrosstalkMax, 3)), camera continuity min $([Math]::Round([double]$row.cameraContinuityMin, 3)), song arc max $([Math]::Round([double]$row.songArcMax, 3))</p></div>")
}
[void]$html.AppendLine("</div>")
[void]$html.AppendLine("<h2>Metrics</h2><table><tr>")
foreach ($property in $summaryRows[0].PSObject.Properties.Name) {
    if ($property -in @("preview", "timeline")) { continue }
    [void]$html.AppendLine("<th>$property</th>")
}
[void]$html.AppendLine("</tr>")
foreach ($row in $summaryRows) {
    [void]$html.AppendLine("<tr>")
    foreach ($property in $row.PSObject.Properties.Name) {
        if ($property -in @("preview", "timeline")) { continue }
        [void]$html.AppendLine("<td>$($row.$property)</td>")
    }
    [void]$html.AppendLine("</tr>")
}
[void]$html.AppendLine("</table>")
[System.IO.File]::WriteAllText($htmlPath, $html.ToString(), [System.Text.Encoding]::UTF8)

Write-Host "Scene QA output: $OutputRoot"
Write-Host "Summary: $summaryPath"
if ($contactSheetWritten) {
    Write-Host "Contact sheet: $contactSheetPath"
}
Write-Host "HTML: $htmlPath"

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    throw "Scene QA failed $($failures.Count) gate(s)."
}
