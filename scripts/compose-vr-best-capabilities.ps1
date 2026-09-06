[CmdletBinding()]
param(
    [string]$CombatVideo = (Join-Path $PSScriptRoot '..\local\fnvxr-controller-full-reload-extra-shots-mobile.mp4'),
    [string]$MirrorDirectory = (Join-Path $PSScriptRoot '..\local\product-runs\20260810-071444-966-96da3249531a\headset-mirror'),
    [string]$OutputVideo = (Join-Path $PSScriptRoot '..\output\fnvxr-vr-best-capabilities-20260810.mp4')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$combat = (Resolve-Path -LiteralPath $CombatVideo).Path
$mirror = (Resolve-Path -LiteralPath $MirrorDirectory).Path
$output = [IO.Path]::GetFullPath($OutputVideo)
$outputDirectory = Split-Path -Parent $output
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
$speechDirectory = Join-Path $outputDirectory 'vr-best-capabilities-audio'
[IO.Directory]::CreateDirectory($speechDirectory) | Out-Null
$narration = Join-Path $speechDirectory 'narration.wav'

Add-Type -AssemblyName System.Speech
$speaker = [System.Speech.Synthesis.SpeechSynthesizer]::new()
try {
    $speaker.Rate = 1
    $speaker.Volume = 100
    $speaker.SetOutputToWaveFile($narration)
    $speaker.Speak(
        'Here is the current F N V V R proof. First, authentic retail engine footage: independent head and body motion, stick locomotion, wide controller aiming, firing, an empty magazine, an engine reload, and follow-up shots. ' +
        'Next, the native wrist Pip-Boy opens from the controller. The right stick navigates actual inventory rows. A selects Power Fist, retail form zero x zero zero zero zero four three four seven, and the engine equips that selected object. ' +
        'B closes the Pip-Boy. The right hand now carries the Power Fist while the left glove, fingers, forearm, and wrist Pip-Boy remain visible. The final strikes are controller driven. ' +
        'No desktop input or fabricated gameplay was used. This isolated Open X R simulator run is proven. Physical headset sign-off is still pending.'
    )
} finally {
    $speaker.Dispose()
}

$font = 'C\:/Windows/Fonts/segoeui.ttf'
$bold = 'C\:/Windows/Fonts/seguisb.ttf'
$mirrorPattern = Join-Path $mirror 'pair_%06d_left.png'
$filter = @"
[0:v]trim=duration=19.2,setpts=PTS-STARTPTS,fps=30,scale=1920:540:flags=lanczos,pad=1920:1080:0:270:color=0x071018[v0];
[1:v]trim=duration=10.7,setpts=PTS-STARTPTS,fps=30,scale=1920:1080:flags=lanczos[v1];
[v0][v1]concat=n=2:v=1:a=0,
drawbox=x=0:y=0:w=iw:h=142:color=0x071018@0.94:t=fill,
drawbox=x=0:y=940:w=iw:h=140:color=0x071018@0.94:t=fill,
drawbox=x=48:y=32:w=9:h=72:color=0x45D4FF@1:t=fill,
drawtext=fontfile='$bold':text='FNVVR | BEST CURRENT VR CAPABILITY PROOF':fontcolor=white:fontsize=43:x=78:y=25,
drawtext=fontfile='$font':text='AUTHENTIC RETAIL ENGINE OUTPUT | SOUND + NARRATION | NO DESKTOP INPUT':fontcolor=0x8FDFFF:fontsize=23:x=80:y=83,
drawtext=fontfile='$font':text='SIMULATOR-PROVEN | PHYSICAL HEADSET SIGN-OFF PENDING':fontcolor=0xFFD37A:fontsize=23:x=(w-text_w)/2:y=1034,
drawtext=fontfile='$bold':text='HEAD + BODY MOTION | STICK LOCOMOTION':fontcolor=white:fontsize=34:x=66:y=188:enable='between(t,0,5.0)',
drawtext=fontfile='$bold':text='CONTROLLER AIM | FIRE | EMPTY MAGAZINE':fontcolor=white:fontsize=34:x=66:y=188:enable='between(t,5.0,14.5)',
drawtext=fontfile='$bold':text='ENGINE RELOAD | FOLLOW-UP SHOTS':fontcolor=white:fontsize=34:x=66:y=188:enable='between(t,14.5,19.2)',
drawtext=fontfile='$bold':text='LEFT GRIP + RIGHT MENU | NATIVE WRIST PIP-BOY':fontcolor=white:fontsize=34:x=66:y=170:enable='between(t,19.2,21.4)',
drawtext=fontfile='$bold':text='RIGHT STICK | REAL INVENTORY ROWS | POWER FIST':fontcolor=white:fontsize=34:x=66:y=170:enable='between(t,21.4,24.0)',
drawtext=fontfile='$bold':text='A | EQUIP SELECTED RETAIL FORM 0x00004347':fontcolor=white:fontsize=34:x=66:y=170:enable='between(t,24.0,25.2)',
drawtext=fontfile='$bold':text='B | CLOSE | HANDS + FINGERS + POWER FIST':fontcolor=white:fontsize=34:x=66:y=170:enable='between(t,25.2,27.2)',
drawtext=fontfile='$bold':text='RIGHT TRIGGER | CONTROLLER-DRIVEN MELEE':fontcolor=white:fontsize=34:x=66:y=170:enable='between(t,27.2,29.9)',
drawbox=x=48:y=965:w=560:h=55:color=0x123044@0.95:t=fill,
drawtext=fontfile='$bold':text='RETAIL WORLD + NATIVE UI':fontcolor=0x65E5A5:fontsize=25:x=72:y=977,
drawbox=x=632:y=965:w=590:h=55:color=0x123044@0.95:t=fill,
drawtext=fontfile='$bold':text='CONTROLLER-DRIVEN HANDOFF':fontcolor=0x65E5A5:fontsize=25:x=656:y=977,
drawbox=x=1246:y=965:w=626:h=55:color=0x123044@0.95:t=fill,
drawtext=fontfile='$bold':text='NO FABRICATED GAMEPLAY':fontcolor=0x65E5A5:fontsize=25:x=1270:y=977[v];
[0:a]atrim=duration=19.2,asetpts=PTS-STARTPTS,volume=0.78[game];
anullsrc=r=48000:cl=stereo,atrim=duration=10.7[silent];
[game][silent]concat=n=2:v=0:a=1[base];
[2:a]adelay=350|350,volume=1.18,highpass=f=90,lowpass=f=10500[narr];
[base][narr]amix=inputs=2:duration=first:dropout_transition=0,alimiter=limit=0.95[a]
"@ -replace "`r?`n", ''

& $ffmpeg -y -hide_banner -loglevel warning `
    -i $combat `
    -framerate 30 -start_number 100 -i $mirrorPattern `
    -i $narration `
    -filter_complex $filter `
    -map '[v]' -map '[a]' `
    -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p `
    -c:a aac -b:a 192k -ar 48000 -movflags +faststart `
    $output
if ($LASTEXITCODE -ne 0) {
    throw "ffmpeg failed with exit code $LASTEXITCODE"
}

$output
