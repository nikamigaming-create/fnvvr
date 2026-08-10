[CmdletBinding()]
param(
    [string]$RunDirectory = (Join-Path $PSScriptRoot '..\local\product-runs\20260809-211305-358-09f147c46f1e'),
    [string]$OutputVideo = (Join-Path $PSScriptRoot '..\output\fnvxr-hands-pipboy-gate-assessment.mp4')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$run = (Resolve-Path -LiteralPath $RunDirectory).Path
$mirror = Join-Path $run 'headset-mirror'
$leftPattern = Join-Path $mirror 'pair_%06d_left.png'
$rightPattern = Join-Path $mirror 'pair_%06d_right.png'
$output = [IO.Path]::GetFullPath($OutputVideo)
$outputDirectory = Split-Path -Parent $output
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$audioDirectory = Join-Path $outputDirectory 'vr-showcase-audio'
[IO.Directory]::CreateDirectory($audioDirectory) | Out-Null
$narration = Join-Path $audioDirectory 'hands-pipboy-gate.wav'

Add-Type -AssemblyName System.Speech
$speaker = [System.Speech.Synthesis.SpeechSynthesizer]::new()
try {
    $speaker.Rate = 1
    $speaker.Volume = 100
    $speaker.SetOutputToWaveFile($narration)
    $speaker.Speak(
        'This is the exact retail root test. The authentic Fallout left forearm and hand, Pip-Boy housing, and equipped pistol are present in binocular Open X R output. ' +
        'Head and controller motion, movement, firing, and reload are proven. ' +
        'The live wrist menu, inventory selection, and ranged-to-melee hand swap are not yet proven. This video fails that final gate on purpose instead of faking it.'
    )
} finally {
    $speaker.Dispose()
}

$ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
$font = 'C\:/Windows/Fonts/segoeui.ttf'
$bold = 'C\:/Windows/Fonts/seguisb.ttf'
$filter = @"
[0:v][1:v]hstack=inputs=2,scale=1920:720:flags=lanczos,pad=1920:1080:0:180:color=0x071018,
drawbox=x=0:y=0:w=iw:h=155:color=0x071018@1:t=fill,
drawbox=x=0:y=900:w=iw:h=180:color=0x071018@1:t=fill,
drawbox=x=52:y=36:w=10:h=76:color=0x45D4FF@1:t=fill,
drawtext=fontfile='$bold':text='EXACT RETAIL HAND + PIP-BOY ROOT ASSESSMENT':fontcolor=white:fontsize=42:x=84:y=30,
drawtext=fontfile='$font':text='RUN 20260809-211305-358-09f147c46f1e  |  LEFT EYE + RIGHT EYE':fontcolor=0x8FDFFF:fontsize=24:x=86:y=91,
drawbox=x=54:y=925:w=850:h=105:color=0x123044@0.95:t=fill,
drawtext=fontfile='$bold':text='PROVEN NOW':fontcolor=0x65E5A5:fontsize=29:x=80:y=939,
drawtext=fontfile='$font':text='Retail forearm / hand / Pip-Boy housing / pistol in stereo':fontcolor=white:fontsize=23:x=80:y=980,
drawbox=x=928:y=925:w=938:h=105:color=0x3B2025@0.97:t=fill,
drawtext=fontfile='$bold':text='NOT YET PROVEN':fontcolor=0xFF9B8A:fontsize=29:x=954:y=939,
drawtext=fontfile='$font':text='Live wrist UI selection + ranged / melee model swap':fontcolor=white:fontsize=23:x=954:y=980,
drawtext=fontfile='$font':text='HEADLESS SIMULATOR EVIDENCE - NO DESKTOP CONTROL':fontcolor=0xFFD37A:fontsize=22:x=(w-text_w)/2:y=1042[v]
"@ -replace "`r?`n", ''

& $ffmpeg -y -hide_banner -loglevel warning `
    -framerate 6 -start_number 60 -i $leftPattern `
    -framerate 6 -start_number 60 -i $rightPattern `
    -i $narration -t 22 `
    -filter_complex $filter `
    -map '[v]' -map '2:a' `
    -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -r 30 `
    -c:a aac -b:a 192k -ar 48000 -movflags +faststart `
    $output
if ($LASTEXITCODE -ne 0) {
    throw "ffmpeg failed with exit code $LASTEXITCODE"
}

$output
