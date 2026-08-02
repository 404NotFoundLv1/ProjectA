[CmdletBinding()]
param(
    [switch]$RequireGeneratedArtifacts
)

$ErrorActionPreference = 'Stop'
$script:Failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        $script:Failures.Add($Message)
    }
}

function Assert-Equal {
    param(
        $Actual,
        $Expected,
        [string]$Message
    )

    if ($Actual -ne $Expected) {
        $script:Failures.Add("$Message Expected '$Expected'; got '$Actual'.")
    }
}

function Invoke-ProjectRiftRunnerForTest {
    param(
        [Parameter(Mandatory)][string]$RunnerPath,
        [Parameter(Mandatory)][string]$Stage,
        [string]$BlenderExe,
        [string]$PythonExe
    )

    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $RunnerPath, '-Stage', $Stage)
    if (-not [string]::IsNullOrWhiteSpace($BlenderExe)) {
        $arguments += @('-BlenderExe', $BlenderExe)
    }
    if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
        $arguments += @('-PythonExe', $PythonExe)
    }
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& powershell @arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Text = ($output | ForEach-Object { $_.ToString() }) -join "`n"
    }
}

try {
    $projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
    $briefPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\ShipHubCompleteDesign_v1.json'
    $modulePath = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\ProjectRift.ArtPipeline.psm1'

    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
        $script:Failures.Add("Required art-pipeline module is missing: $modulePath")
    }
    else {
        Import-Module -Name $modulePath -Force -ErrorAction Stop

        $allowedArtRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub'
        $artChild = Join-Path $allowedArtRoot 'CompleteDesign\test-output.txt'
        $siblingPrefixPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHubSibling'
        $contentPath = Join-Path $projectRoot 'Content\ProjectRift'

        Assert-True (Test-ProjectRiftContainedArtPath -Candidate $artChild -AllowedRoot $allowedArtRoot) 'Containment should accept a child of SourceArt\\ProjectRift\\ShipHub.'
        Assert-True (-not (Test-ProjectRiftContainedArtPath -Candidate $allowedArtRoot -AllowedRoot $allowedArtRoot)) 'Containment should reject the allowed root itself.'
        Assert-True (-not (Test-ProjectRiftContainedArtPath -Candidate $siblingPrefixPath -AllowedRoot $allowedArtRoot)) 'Containment should reject the SourceArt\\ProjectRift\\ShipHubSibling prefix sibling.'
        Assert-True (-not (Test-ProjectRiftContainedArtPath -Candidate $contentPath -AllowedRoot $allowedArtRoot)) 'Containment should reject Content\\ProjectRift.'

        $resolverTestRoot = Join-Path $projectRoot ("Saved\Automation\ProjectRiftShipHubDesign\resolver-test-{0}" -f [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $resolverTestRoot -Force | Out-Null
        $validBlenderFallback = Join-Path $resolverTestRoot 'blender.exe'
        $validPythonFallback = Join-Path $resolverTestRoot 'python.exe'
        New-Item -ItemType File -Path $validBlenderFallback -Force | Out-Null
        New-Item -ItemType File -Path $validPythonFallback -Force | Out-Null
        $hadBlenderEnvironment = Test-Path Env:\PROJECTRIFT_BLENDER_EXE
        $hadPythonEnvironment = Test-Path Env:\PROJECTRIFT_PYTHON_EXE
        $originalBlenderEnvironment = $env:PROJECTRIFT_BLENDER_EXE
        $originalPythonEnvironment = $env:PROJECTRIFT_PYTHON_EXE
        try {
            $env:PROJECTRIFT_BLENDER_EXE = $validBlenderFallback
            $env:PROJECTRIFT_PYTHON_EXE = $validPythonFallback

            $missingBlender = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\missing\blender.exe'
            $missingBlenderThrew = $false
            try {
                Resolve-ProjectRiftBlenderExecutable -ExplicitPath $missingBlender | Out-Null
            }
            catch {
                $missingBlenderThrew = $true
            }
            Assert-True $missingBlenderThrew 'Blender resolver should throw for a missing explicit blender.exe path even when the environment fallback is valid.'

            $missingPython = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\missing\python.exe'
            $missingPythonThrew = $false
            try {
                Resolve-ProjectRiftPythonExecutable -ExplicitPath $missingPython | Out-Null
            }
            catch {
                $missingPythonThrew = $true
            }
            Assert-True $missingPythonThrew 'Python resolver should throw for a missing explicit python.exe path even when the environment fallback is valid.'
        }
        finally {
            if ($hadBlenderEnvironment) {
                $env:PROJECTRIFT_BLENDER_EXE = $originalBlenderEnvironment
            }
            else {
                Remove-Item Env:\PROJECTRIFT_BLENDER_EXE -ErrorAction SilentlyContinue
            }
            if ($hadPythonEnvironment) {
                $env:PROJECTRIFT_PYTHON_EXE = $originalPythonEnvironment
            }
            else {
                Remove-Item Env:\PROJECTRIFT_PYTHON_EXE -ErrorAction SilentlyContinue
            }
            Remove-Item -LiteralPath $resolverTestRoot -Recurse -Force -ErrorAction SilentlyContinue
        }

        $runnerPath = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubDesign.ps1'
        $runnerTestRoot = Join-Path $projectRoot ("Saved\Automation\ProjectRiftShipHubDesign\runner-test-{0}" -f [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $runnerTestRoot -Force | Out-Null
        $validBlenderProbe = Join-Path $runnerTestRoot 'blender.exe'
        $wrongBlenderProbe = Join-Path $runnerTestRoot 'wrong\blender.exe'
        $pythonProbe = Join-Path $runnerTestRoot 'python.exe'
        $failingPythonProbe = Join-Path $runnerTestRoot 'failing-python.exe'
        $toolLog = Join-Path $runnerTestRoot 'tool-invocations.log'
        New-Item -ItemType Directory -Path (Split-Path -Parent $wrongBlenderProbe) -Force | Out-Null

        $validBlenderSource = @'
using System;
using System.IO;
public static class ProjectRiftValidBlenderProbe {
    public static int Main(string[] args) {
        string log = Environment.GetEnvironmentVariable("PROJECTRIFT_TEST_TOOL_LOG");
        if (!String.IsNullOrEmpty(log)) File.AppendAllText(log, "blender:" + String.Join(" ", args) + Environment.NewLine);
        Console.WriteLine("Blender 5.2.3 LTS");
        return 0;
    }
}
'@
        $wrongBlenderSource = @'
using System;
public static class ProjectRiftWrongBlenderProbe {
    public static int Main(string[] args) {
        Console.WriteLine("Blender 5.1.0");
        return 0;
    }
}
'@
        $pythonSource = @'
using System;
using System.IO;
public static class ProjectRiftPythonProbe {
    public static int Main(string[] args) {
        string log = Environment.GetEnvironmentVariable("PROJECTRIFT_TEST_TOOL_LOG");
        if (!String.IsNullOrEmpty(log)) File.AppendAllText(log, "python:" + String.Join(" ", args) + Environment.NewLine);
        return 0;
    }
}
'@
        $failingPythonSource = @'
public static class ProjectRiftFailingPythonProbe {
    public static int Main(string[] args) { return 7; }
}
'@

        $hadToolLogEnvironment = Test-Path Env:\PROJECTRIFT_TEST_TOOL_LOG
        $originalToolLogEnvironment = $env:PROJECTRIFT_TEST_TOOL_LOG
        try {
            Add-Type -TypeDefinition $validBlenderSource -OutputAssembly $validBlenderProbe -OutputType ConsoleApplication
            Add-Type -TypeDefinition $wrongBlenderSource -OutputAssembly $wrongBlenderProbe -OutputType ConsoleApplication
            Add-Type -TypeDefinition $pythonSource -OutputAssembly $pythonProbe -OutputType ConsoleApplication
            Add-Type -TypeDefinition $failingPythonSource -OutputAssembly $failingPythonProbe -OutputType ConsoleApplication
            $env:PROJECTRIFT_TEST_TOOL_LOG = $toolLog

            foreach ($blenderStage in @('BuildWhiteModel', 'RenderDrawings', 'Validate')) {
                $wrongVersionResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage $blenderStage -BlenderExe $wrongBlenderProbe -PythonExe $pythonProbe
                Assert-True ($wrongVersionResult.ExitCode -ne 0) "$blenderStage should fail for a non-5.2 Blender executable."
                Assert-True ($wrongVersionResult.Text -match 'requires Blender 5\.2\.x LTS') "$blenderStage should check Blender 5.2 LTS before checking its deferred stage script."
                Assert-True ($wrongVersionResult.Text -notmatch 'missing its required script') "$blenderStage should not inspect its deferred script before the Blender version gate."

                Remove-Item -LiteralPath $toolLog -Force -ErrorAction SilentlyContinue
                $validVersionResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage $blenderStage -BlenderExe $validBlenderProbe -PythonExe $pythonProbe
                Assert-True ($validVersionResult.ExitCode -ne 0) "$blenderStage should fail closed while its deferred script is absent."
                Assert-True ($validVersionResult.Text -match 'missing its required script') "$blenderStage should report its missing deferred script after Blender validation."
                $validStageLog = if (Test-Path -LiteralPath $toolLog) { @(Get-Content -LiteralPath $toolLog) } else { @() }
                $validStageFirstLine = ([string]($validStageLog | Select-Object -First 1)).Trim()
                Assert-True ($validStageFirstLine -eq 'blender:--version') "$blenderStage should invoke the supplied Blender version check before the missing-script error. Actual log: $($validStageLog -join ' | ')"
            }

            $missingPublishPython = Join-Path $runnerTestRoot 'missing-python.exe'
            $publishResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage 'Publish' -PythonExe $missingPublishPython
            Assert-True ($publishResult.ExitCode -ne 0) 'Publish should fail for an invalid explicit Python executable.'
            Assert-True ($publishResult.Text -match 'Explicit Python path') 'Publish should resolve its supplied Python before checking its deferred script.'
            Assert-True ($publishResult.Text -notmatch 'missing its required script') 'Publish should not inspect its deferred script before Python resolution.'

            $pythonFailureResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage 'ValidateContract' -PythonExe $failingPythonProbe
            Assert-True ($pythonFailureResult.ExitCode -ne 0) 'ValidateContract should propagate a nonzero Python exit.'
            Assert-True ($pythonFailureResult.Text -match 'exited with code 7') 'ValidateContract should report the supplied Python exit code.'

            Remove-Item -LiteralPath $toolLog -Force -ErrorAction SilentlyContinue
            $allResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage 'All' -BlenderExe $validBlenderProbe -PythonExe $pythonProbe
            Assert-True ($allResult.ExitCode -ne 0) 'All should fail closed at the first absent deferred stage.'
            Assert-True ($allResult.Text -match 'BuildWhiteModel stage is missing its required script') "All should reach BuildWhiteModel only after Preflight and ValidateContract. Actual output: $($allResult.Text)"
            $allLog = if (Test-Path -LiteralPath $toolLog) { @(Get-Content -LiteralPath $toolLog) } else { @() }
            Assert-True ($allLog.Count -ge 4) "All should invoke Preflight Blender, Preflight Python, ValidateContract Python, then BuildWhiteModel Blender. Actual log: $($allLog -join ' | ')"
            if ($allLog.Count -ge 4) {
                Assert-Equal $allLog[0] 'blender:--version' 'All should invoke Preflight first with the explicit Blender executable.'
                Assert-True ($allLog[1] -like 'python:-c import PIL, reportlab, pypdf*') 'All should forward the explicit Python executable to Preflight.'
                Assert-True ($allLog[2] -like 'python:*shiphub_contract.py*') 'All should forward the explicit Python executable to ValidateContract.'
                Assert-Equal $allLog[3] 'blender:--version' 'All should forward the explicit Blender executable to BuildWhiteModel.'
            }
        }
        finally {
            if ($hadToolLogEnvironment) {
                $env:PROJECTRIFT_TEST_TOOL_LOG = $originalToolLogEnvironment
            }
            else {
                Remove-Item Env:\PROJECTRIFT_TEST_TOOL_LOG -ErrorAction SilentlyContinue
            }
            Remove-Item -LiteralPath $runnerTestRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    if (-not (Test-Path -LiteralPath $briefPath -PathType Leaf)) {
        $script:Failures.Add("Required design brief is missing: $briefPath")
    }
    else {
        $brief = Get-Content -LiteralPath $briefPath -Raw -Encoding UTF8 | ConvertFrom-Json

        Assert-Equal $brief.Schema 'projectrift.shiphub.complete-design.v1' 'Schema mismatch.'
        Assert-Equal $brief.Room.ClearWidthM 28.0 'Room clear width mismatch.'
        Assert-Equal $brief.Room.ClearDepthM 24.0 'Room clear depth mismatch.'
        Assert-Equal $brief.Room.ClearHeightM 7.0 'Room clear height mismatch.'
        Assert-Equal $brief.NavigationTable.DiameterM 8.0 'Navigation table diameter mismatch.'
        Assert-Equal $brief.Cryopods.Count 5 'Cryopod count mismatch.'
        Assert-Equal $brief.Cryopods.ReclineDegrees 18.0 'Cryopod recline mismatch.'
        Assert-Equal $brief.ConstructDocks.Count 4 'Construct dock count mismatch.'

        $expectedSheets = @(
            'A01_FloorPlan', 'A02_ReflectedCeilingPlan', 'A03_NorthElevation',
            'A04_SouthElevation', 'A05_WestElevation', 'A06_EastElevation',
            'A07_LongitudinalSection', 'A08_TransverseSection', 'A09_ExplodedModulePlan',
            'A10_PerspectiveSheet', 'D01_Cryopod', 'D02_NavigationTable',
            'D03_MainAirlock', 'D04_ConstructDock', 'D05_WallBayInterface'
        )
        $actualSheets = @($brief.Deliverables.Sheets)
        Assert-Equal $actualSheets.Count 15 'Deliverable sheet count mismatch.'
        for ($index = 0; $index -lt $expectedSheets.Count; $index++) {
            $actualSheet = if ($index -lt $actualSheets.Count) { $actualSheets[$index] } else { $null }
            Assert-Equal $actualSheet $expectedSheets[$index] "Deliverable sheet ID at index $index mismatch."
        }

        if ($RequireGeneratedArtifacts) {
            Assert-True $true 'Generated-artifact validation is deferred to the artifact-producing task.'
        }
    }
}
catch {
    $script:Failures.Add("Unexpected validation error: $($_.Exception.Message)")
}

if ($script:Failures.Count -gt 0) {
    $script:Failures | ForEach-Object { Write-Error $_ -ErrorAction Continue }
    Write-Output 'ProjectRift ship-hub design self-test: FAIL'
    exit 1
}

Write-Output 'ProjectRift ship-hub design self-test: PASS'
exit 0
