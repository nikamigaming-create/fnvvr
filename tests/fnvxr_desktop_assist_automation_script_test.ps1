param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$launcher = Join-Path $SourceRoot "scripts\start-desktop-assist.ps1"
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "Desktop-assist launcher is missing: $launcher"
}

$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $launcher,
    [ref]$tokens,
    [ref]$parseErrors)
if ($parseErrors.Count -ne 0) {
    $details = $parseErrors | ForEach-Object {
        "line=$($_.Extent.StartLineNumber) message=$($_.Message)"
    }
    throw "Desktop-assist launcher has PowerShell parse errors: $($details -join '; ')"
}

$source = Get-Content -LiteralPath $launcher -Raw
foreach ($required in @(
    '[switch]$AutomateAcceptance',
    '-AutomateAcceptance requires -RunAcceptanceTrial',
    'FNVXR_DESKTOP_ASSIST_AUTOMATION = "1"',
    'load FNVXR_HostExitRecovery',
    'Invoke-DesktopAssistAutomatedMenuRoundTrip',
    'Send-DesktopAssistEscape',
    'Get-DesktopAssistAcceptanceReportEvidence',
    'bodyPositionUnits',
    'desktopAssistHeadBodyDecoupled',
    'headTranslationBodyDecoupled')) {
    if (-not $source.Contains($required)) {
        throw "Desktop-assist automation launcher lost required contract: $required"
    }
}

$evidenceFunction = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] `
            -and $node.Name -eq 'Get-DesktopAssistAcceptanceReportEvidence'
    }, $true)
if ($null -eq $evidenceFunction) {
    throw 'Desktop-assist launcher lost its acceptance-report evidence parser.'
}
. ([scriptblock]::Create($evidenceFunction.Extent.Text))

function New-DesktopAssistAcceptanceReport {
    param(
        [bool]$HeadTranslationBodyDecoupled = $true,
        [double]$BodyPositionToleranceUnits = 0.25,
        [string]$CameraEvidenceSource = 'desktop-assist-local-camera'
    )

    return [pscustomobject]@{
        cameraEvidenceSource = $CameraEvidenceSource
        requestedChecksPass = $true
        desktopAssistHeadBodyDecoupled = $true
        headTranslationBodyDecoupled = $HeadTranslationBodyDecoupled
        runtimeUiTransitionObserved = $true
        desktopAssistUiQuadPixelsVerifiedObserved = $true
        desktopAssistUiQuadInvalidatedAfterUiObserved = $true
        desktopAssistUiQuadTransitionObserved = $true
        thresholds = [pscustomobject]@{
            bodyPositionUnits = $BodyPositionToleranceUnits
        }
    }
}

$validEvidence = Get-DesktopAssistAcceptanceReportEvidence `
    (New-DesktopAssistAcceptanceReport)
if (-not $validEvidence.complete) {
    throw 'Desktop-assist acceptance parser rejected complete local-camera evidence.'
}

$bodyLeakEvidence = Get-DesktopAssistAcceptanceReportEvidence `
    (New-DesktopAssistAcceptanceReport -HeadTranslationBodyDecoupled $false)
if ($bodyLeakEvidence.complete) {
    throw 'Desktop-assist acceptance parser accepted head translation leaking into the body root.'
}

$looseToleranceEvidence = Get-DesktopAssistAcceptanceReportEvidence `
    (New-DesktopAssistAcceptanceReport -BodyPositionToleranceUnits 0.26)
if ($looseToleranceEvidence.complete) {
    throw 'Desktop-assist acceptance parser accepted an unsafe body-position tolerance.'
}

$wrongSourceEvidence = Get-DesktopAssistAcceptanceReportEvidence `
    (New-DesktopAssistAcceptanceReport -CameraEvidenceSource 'world-camera')
if ($wrongSourceEvidence.complete) {
    throw 'Desktop-assist acceptance parser accepted a non-local camera evidence source.'
}

$match = [regex]::Match(
    $source,
    "Add-Type -Language CSharp -TypeDefinition @'\r?\n(?<code>[\s\S]*?)\r?\n'@",
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $match.Success) {
    throw "Could not locate the desktop-assist native Escape helper source."
}
$nativeSource = $match.Groups['code'].Value
$foregroundCheck = $nativeSource.IndexOf("foregroundProcessId != processId")
$foregroundWindowCheck = $nativeSource.IndexOf("foreground != window")
$sendInput = $nativeSource.IndexOf("SendInput(2")
if ($foregroundCheck -lt 0 -or $foregroundWindowCheck -lt 0 -or $sendInput -lt 0 `
    -or $foregroundCheck -gt $sendInput -or $foregroundWindowCheck -gt $sendInput) {
    throw "Desktop-assist Escape helper can inject before proving the selected Fallout window is foreground."
}
if (-not $nativeSource.Contains("return inserted == 2;")) {
    throw "Desktop-assist Escape helper does not require both key events to be injected."
}

Add-Type -Language CSharp -TypeDefinition $nativeSource
$nativeType = "Fnvxr.DesktopAssist.NativeInput" -as [type]
if (-not $nativeType) {
    throw "Desktop-assist native Escape helper did not compile."
}
if (-not $nativeType.GetMethod("SendEscapeToProcess")) {
    throw "Desktop-assist native Escape helper lost its bounded process-targeted API."
}

Write-Output "desktop assist automation script test passed"
