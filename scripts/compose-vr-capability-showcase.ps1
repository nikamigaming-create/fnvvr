[CmdletBinding()]
param(
    [string]$SourceVideo = (Join-Path $PSScriptRoot '..\local\fnvxr-controller-full-reload-extra-shots-mobile.mp4'),
    [string]$OutputVideo = (Join-Path $PSScriptRoot '..\output\fnvxr-vr-capability-showcase.mp4')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$source = (Resolve-Path -LiteralPath $SourceVideo).Path
$output = [IO.Path]::GetFullPath($OutputVideo)
$outputDirectory = Split-Path -Parent $output
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
$speechDirectory = Join-Path $outputDirectory 'vr-showcase-audio'
[IO.Directory]::CreateDirectory($speechDirectory) | Out-Null
$narration = Join-Path $speechDirectory 'narration.wav'

Add-Type -AssemblyName System.Speech
$speaker = [System.Speech.Synthesis.SpeechSynthesizer]::new()
try {
    $speaker.Rate = 2
    $speaker.Volume = 100
    $speaker.SetOutputToWaveFile($narration)
    $speaker.Speak(
        'Fallout New Vegas, rendered in true binocular Open X R. ' +
        'Here, independent six degree head motion runs with left stick movement, turn input, and wide tracked pistol aiming. ' +
        'The retail engine fires, empties the magazine, reloads, then fires again. ' +
        'Simulator proven; physical headset sign off pending.'
    )
} finally {
    $speaker.Dispose()
}

$font = 'C\:/Windows/Fonts/segoeui.ttf'
$bold = 'C\:/Windows/Fonts/seguisb.ttf'
$filter = @"
[0:v]fps=30,scale=1920:540:flags=lanczos,pad=1920:1080:0:270:color=0x071018,
drawbox=x=0:y=0:w=iw:h=185:color=0x071018@1:t=fill,
drawbox=x=0:y=895:w=iw:h=185:color=0x071018@1:t=fill,
drawbox=x=56:y=43:w=10:h=86:color=0x45D4FF@1:t=fill,
drawtext=fontfile='$bold':text='FNVVR  |  CURRENT CAPABILITY PROOF':fontcolor=white:fontsize=50:x=90:y=38,
drawtext=fontfile='$font':text='AUTHENTIC RETAIL ENGINE FRAMES  •  OPENXR HEADLESS SIMULATOR':fontcolor=0x8FDFFF:fontsize=25:x=92:y=108,
drawbox=x=58:y=922:w=535:h=100:color=0x123044@0.92:t=fill,
drawtext=fontfile='$bold':text='BINOCULAR 3D':fontcolor=0x65E5A5:fontsize=29:x=84:y=940,
drawtext=fontfile='$font':text='Distinct left / right eye views':fontcolor=white:fontsize=23:x=84:y=980,
drawbox=x=616:y=922:w=535:h=100:color=0x123044@0.92:t=fill,
drawtext=fontfile='$bold':text='6DoF + STICK MOTION':fontcolor=0x65E5A5:fontsize=29:x=642:y=940,
drawtext=fontfile='$font':text='Head look • stick move • turn input':fontcolor=white:fontsize=23:x=642:y=980,
drawbox=x=1174:y=922:w=688:h=100:color=0x123044@0.92:t=fill,
drawtext=fontfile='$bold':text='TRACKED COMBAT LOOP':fontcolor=0x65E5A5:fontsize=29:x=1200:y=940,
drawtext=fontfile='$font':text='Aim • fire • engine reload • fire':fontcolor=white:fontsize=23:x=1200:y=980,
drawtext=fontfile='$font':text='SIMULATOR-PROVEN  |  PHYSICAL HEADSET SIGN-OFF PENDING':fontcolor=0xFFD37A:fontsize=24:x=(w-text_w)/2:y=1035,
drawtext=fontfile='$bold':text='INDEPENDENT HEAD + BODY MOTION':fontcolor=white:fontsize=34:x=70:y=220:enable='between(t,0,5.0)',
drawtext=fontfile='$bold':text='WIDE 3D CONTROLLER / WEAPON AIM':fontcolor=white:fontsize=34:x=70:y=220:enable='between(t,5.0,10.0)',
drawtext=fontfile='$bold':text='REAL FIRING + MAGAZINE EMPTY':fontcolor=white:fontsize=34:x=70:y=220:enable='between(t,10.0,14.5)',
drawtext=fontfile='$bold':text='ENGINE RELOAD + FOLLOW-UP SHOTS':fontcolor=white:fontsize=34:x=70:y=220:enable='between(t,14.5,19.2)'[v];
[0:a]volume=0.78[game];
[1:a]adelay=450|450,volume=1.25,highpass=f=90,lowpass=f=10500[narr];
[game][narr]amix=inputs=2:duration=first:dropout_transition=0,alimiter=limit=0.95[a]
"@ -replace "`r?`n", ''

& $ffmpeg -y -hide_banner -loglevel warning `
    -i $source -i $narration `
    -filter_complex $filter `
    -map '[v]' -map '[a]' `
    -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p `
    -c:a aac -b:a 192k -ar 48000 -movflags +faststart `
    $output
if ($LASTEXITCODE -ne 0) {
    throw "ffmpeg failed with exit code $LASTEXITCODE"
}

$output
