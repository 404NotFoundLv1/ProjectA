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

function Assert-SequenceEqual {
    param(
        [object[]]$Actual,
        [object[]]$Expected,
        [string]$Message
    )

    Assert-Equal $Actual.Count $Expected.Count "$Message count mismatch."
    for ($index = 0; $index -lt $Expected.Count; $index++) {
        $actualValue = if ($index -lt $Actual.Count) { $Actual[$index] } else { $null }
        Assert-Equal $actualValue $Expected[$index] "$Message at index $index mismatch."
    }
}

function Assert-Near {
    param(
        [double]$Actual,
        [double]$Expected,
        [double]$Tolerance,
        [string]$Message
    )

    if ([Math]::Abs($Actual - $Expected) -gt $Tolerance) {
        $script:Failures.Add("$Message Expected '$Expected' +/- '$Tolerance'; got '$Actual'.")
    }
}

function Assert-LessOrEqual {
    param(
        [double]$Actual,
        [double]$Maximum,
        [double]$Tolerance,
        [string]$Message
    )

    if ($Actual -gt ($Maximum + $Tolerance)) {
        $script:Failures.Add("$Message Expected <= '$Maximum' with tolerance '$Tolerance'; got '$Actual'.")
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
        $failingBuildBlenderProbe = Join-Path $runnerTestRoot 'failing-build\blender.exe'
        $pythonProbe = Join-Path $runnerTestRoot 'python.exe'
        $failingPythonProbe = Join-Path $runnerTestRoot 'failing-python.exe'
        $toolLog = Join-Path $runnerTestRoot 'tool-invocations.log'
        New-Item -ItemType Directory -Path (Split-Path -Parent $wrongBlenderProbe) -Force | Out-Null
        New-Item -ItemType Directory -Path (Split-Path -Parent $failingBuildBlenderProbe) -Force | Out-Null

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
        $failingBuildBlenderSource = @'
using System;
using System.IO;
public static class ProjectRiftFailingBuildBlenderProbe {
    public static int Main(string[] args) {
        string log = Environment.GetEnvironmentVariable("PROJECTRIFT_TEST_TOOL_LOG");
        if (!String.IsNullOrEmpty(log)) File.AppendAllText(log, "blender:" + String.Join(" ", args) + Environment.NewLine);
        if (args.Length == 1 && args[0] == "--version") {
            Console.WriteLine("Blender 5.2.3 LTS");
            return 0;
        }
        return 9;
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
            Add-Type -TypeDefinition $failingBuildBlenderSource -OutputAssembly $failingBuildBlenderProbe -OutputType ConsoleApplication
            Add-Type -TypeDefinition $pythonSource -OutputAssembly $pythonProbe -OutputType ConsoleApplication
            Add-Type -TypeDefinition $failingPythonSource -OutputAssembly $failingPythonProbe -OutputType ConsoleApplication
            $env:PROJECTRIFT_TEST_TOOL_LOG = $toolLog

            $wrongBuildVersionResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage 'BuildWhiteModel' -BlenderExe $wrongBlenderProbe -PythonExe $pythonProbe
            Assert-True ($wrongBuildVersionResult.ExitCode -ne 0) 'BuildWhiteModel should fail for a non-5.2 Blender executable.'
            Assert-True ($wrongBuildVersionResult.Text -match 'requires Blender 5\.2\.x LTS') 'BuildWhiteModel should check Blender 5.2 LTS before checking its build scripts.'

            Remove-Item -LiteralPath $toolLog -Force -ErrorAction SilentlyContinue
            $failingBuildResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage 'BuildWhiteModel' -BlenderExe $failingBuildBlenderProbe -PythonExe $pythonProbe
            Assert-True ($failingBuildResult.ExitCode -ne 0) 'BuildWhiteModel should propagate a nonzero background Blender exit.'
            Assert-True ($failingBuildResult.Text -match 'BuildWhiteModel failed with exit code 9\.') 'BuildWhiteModel should report the exact nonzero background Blender exit code.'
            $failingBuildLog = if (Test-Path -LiteralPath $toolLog) { @(Get-Content -LiteralPath $toolLog) } else { @() }
            Assert-Equal $failingBuildLog.Count 2 'BuildWhiteModel failure probe should run the version gate and one background build.'
            if ($failingBuildLog.Count -eq 2) {
                Assert-Equal $failingBuildLog[0] 'blender:--version' 'BuildWhiteModel failure probe should run the independent version gate first.'
                Assert-True ($failingBuildLog[1] -like 'blender:--background --factory-startup --python *build_shiphub_design.py*') 'BuildWhiteModel failure probe should fail from the background build invocation.'
            }

            Remove-Item -LiteralPath $toolLog -Force -ErrorAction SilentlyContinue
            $validBuildResult = Invoke-ProjectRiftRunnerForTest -RunnerPath $runnerPath -Stage 'BuildWhiteModel' -BlenderExe $validBlenderProbe -PythonExe $pythonProbe
            Assert-Equal $validBuildResult.ExitCode 0 'BuildWhiteModel should invoke Blender successfully when its scripts exist.'
            $validBuildLog = if (Test-Path -LiteralPath $toolLog) { @(Get-Content -LiteralPath $toolLog) } else { @() }
            Assert-Equal $validBuildLog.Count 2 'BuildWhiteModel should run the independent version gate and one background build.'
            if ($validBuildLog.Count -eq 2) {
                Assert-Equal $validBuildLog[0] 'blender:--version' 'BuildWhiteModel should invoke the supplied Blender version check first.'
                Assert-True ($validBuildLog[1] -like 'blender:--background --factory-startup --python *build_shiphub_design.py -- --project-root * --brief *ShipHubCompleteDesign_v1.json --output-root *CompleteDesign') "BuildWhiteModel should invoke the exact background builder contract. Actual log: $($validBuildLog[1])"
            }

            foreach ($blenderStage in @('RenderDrawings', 'Validate')) {
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
            Assert-True ($allResult.Text -match 'RenderDrawings stage is missing its required script') "All should complete BuildWhiteModel and stop at the absent RenderDrawings stage. Actual output: $($allResult.Text)"
            $allLog = if (Test-Path -LiteralPath $toolLog) { @(Get-Content -LiteralPath $toolLog) } else { @() }
            Assert-True ($allLog.Count -ge 6) "All should invoke Preflight, ValidateContract, BuildWhiteModel and the RenderDrawings Blender gate in order. Actual log: $($allLog -join ' | ')"
            if ($allLog.Count -ge 6) {
                Assert-Equal $allLog[0] 'blender:--version' 'All should invoke Preflight first with the explicit Blender executable.'
                Assert-True ($allLog[1] -like 'python:-c import PIL, reportlab, pypdf*') 'All should forward the explicit Python executable to Preflight.'
                Assert-True ($allLog[2] -like 'python:*shiphub_contract.py*') 'All should forward the explicit Python executable to ValidateContract.'
                Assert-Equal $allLog[3] 'blender:--version' 'All should forward the explicit Blender executable to BuildWhiteModel.'
                Assert-True ($allLog[4] -like 'blender:--background --factory-startup --python *build_shiphub_design.py*') 'All should run the background white-model build after its version gate.'
                Assert-Equal $allLog[5] 'blender:--version' 'All should reach the RenderDrawings Blender gate only after BuildWhiteModel.'
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
            $outputRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\CompleteDesign'
            $blendPath = Join-Path $outputRoot 'Blender\SM_ShipHub_Complete_White_v1.blend'
            $fbxPath = Join-Path $outputRoot 'Exports\SM_ShipHub_Complete_White_v1.fbx'
            $glbPath = Join-Path $outputRoot 'Exports\SM_ShipHub_Complete_White_v1.glb'
            $manifestPath = Join-Path $outputRoot 'Reports\layout-manifest.json'
            $requiredArtifacts = @($blendPath, $fbxPath, $glbPath, $manifestPath)

            foreach ($artifactPath in $requiredArtifacts) {
                $artifactExists = Test-Path -LiteralPath $artifactPath -PathType Leaf
                Assert-True $artifactExists "Required generated artifact is missing: $artifactPath"
                if ($artifactExists) {
                    Assert-True ((Get-Item -LiteralPath $artifactPath).Length -gt 0) "Required generated artifact is empty: $artifactPath"
                }
            }
            $unexpectedFiles = @(
                Get-ChildItem -LiteralPath $outputRoot -File -Recurse -Force |
                    Where-Object { $_.FullName -notin $requiredArtifacts }
            )
            Assert-Equal $unexpectedFiles.Count 0 "CompleteDesign must not retain non-contract files: $($unexpectedFiles.FullName -join ', ')"
            $unexpectedWorkDirectories = @(
                Get-ChildItem -LiteralPath $outputRoot -Directory -Force |
                    Where-Object { $_.Name -like '.shiphub-*' }
            )
            Assert-Equal $unexpectedWorkDirectories.Count 0 "CompleteDesign must not retain staging or backup directories: $($unexpectedWorkDirectories.FullName -join ', ')"

            if (Test-Path -LiteralPath $blendPath -PathType Leaf) {
                $blendLength = (Get-Item -LiteralPath $blendPath).Length
                Assert-True ($blendLength -ge 7) 'BLEND artifact must contain at least 7 bytes for signature validation.'
                if ($blendLength -ge 7) {
                    $blendBytes = [IO.File]::ReadAllBytes($blendPath)
                    $isPlainBlend = ([Text.Encoding]::ASCII.GetString($blendBytes, 0, 7) -ceq 'BLENDER')
                    $isZstdBlend = (
                        $blendBytes[0] -eq 0x28 -and $blendBytes[1] -eq 0xB5 -and
                        $blendBytes[2] -eq 0x2F -and $blendBytes[3] -eq 0xFD
                    )
                    Assert-True ($isPlainBlend -or $isZstdBlend) 'BLEND signature must be plain BLENDER or Blender Zstandard compression.'
                }
            }
            if (Test-Path -LiteralPath $fbxPath -PathType Leaf) {
                $fbxLength = (Get-Item -LiteralPath $fbxPath).Length
                Assert-True ($fbxLength -ge 18) 'FBX artifact must contain at least 18 bytes for signature validation.'
                if ($fbxLength -ge 18) {
                    $fbxBytes = [IO.File]::ReadAllBytes($fbxPath)
                    Assert-Equal ([Text.Encoding]::ASCII.GetString($fbxBytes, 0, 18)) 'Kaydara FBX Binary' 'FBX signature mismatch.'
                }
            }
            if (Test-Path -LiteralPath $glbPath -PathType Leaf) {
                $glbLength = (Get-Item -LiteralPath $glbPath).Length
                Assert-True ($glbLength -ge 4) 'GLB artifact must contain at least 4 bytes for signature validation.'
                if ($glbLength -ge 4) {
                    $glbBytes = [IO.File]::ReadAllBytes($glbPath)
                    Assert-Equal ([Text.Encoding]::ASCII.GetString($glbBytes, 0, 4)) 'glTF' 'GLB signature mismatch.'
                }
            }

            if ((Test-Path -LiteralPath $manifestPath -PathType Leaf) -and (Get-Item -LiteralPath $manifestPath).Length -gt 0) {
                $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
                Assert-Equal $manifest.schema 'projectrift.shiphub.white-model-manifest.v1' 'White-model manifest schema mismatch.'
                Assert-True ([string]$manifest.blender.version -match '^5\.2\.') 'White-model manifest Blender version must begin with 5.2.'
                Assert-Equal $manifest.blender.lts $true 'White-model manifest Blender LTS flag mismatch.'
                Assert-True ([string]$manifest.source_brief.sha256 -cmatch '^[0-9a-f]{64}$') 'White-model manifest brief SHA-256 must be lowercase 64-character hexadecimal.'
                $actualBriefHash = (Get-FileHash -LiteralPath $briefPath -Algorithm SHA256).Hash.ToLowerInvariant()
                Assert-Equal $manifest.source_brief.sha256 $actualBriefHash 'White-model manifest brief SHA-256 does not match the actual brief.'

                Assert-SequenceEqual @($manifest.room.clear_dimensions_m) @(28.0, 24.0, 7.0) 'White-model clear dimensions'
                Assert-SequenceEqual @($manifest.room.clear_bounds_m.min) @(-14.0, -12.0, 0.0) 'White-model clear minimum bounds'
                Assert-SequenceEqual @($manifest.room.clear_bounds_m.max) @(14.0, 12.0, 7.0) 'White-model clear maximum bounds'

                $expectedCollections = @(
                    '00_REFERENCE', '10_STRUCTURE', '20_NAV_TABLE', '30_CRYOPODS',
                    '40_AIRLOCK', '50_WEST_BAYS', '60_EAST_BAYS',
                    '70_CONSTRUCT_DOCKS', '80_CEILING', '90_CAMERAS'
                )
                Assert-SequenceEqual @($manifest.collections) $expectedCollections 'White-model collection names'

                $expectedMaterials = @(
                    'MAT_Structure', 'MAT_Interactable', 'MAT_Glass',
                    'MAT_Door', 'MAT_NonWalkable'
                )
                Assert-SequenceEqual @($manifest.materials) $expectedMaterials 'White-model material names'

                $expectedCryopods = @(
                    'SM_ShipHub_Cryopod_01', 'SM_ShipHub_Cryopod_02',
                    'SM_ShipHub_Cryopod_03', 'SM_ShipHub_Cryopod_04',
                    'SM_ShipHub_Cryopod_05'
                )
                Assert-Equal $manifest.assemblies.cryopods.count 5 'White-model cryopod count mismatch.'
                Assert-SequenceEqual @($manifest.assemblies.cryopods.names) $expectedCryopods 'White-model cryopod names'
                Assert-Equal $manifest.assemblies.cryopods.recline_degrees 18.0 'White-model cryopod semantic recline mismatch.'
                $cryopodMembers = @($manifest.assemblies.cryopods.members | Where-Object { $null -ne $_ })
                Assert-Equal $cryopodMembers.Count 5 'White-model measured cryopod member count mismatch.'
                foreach ($podName in $expectedCryopods) {
                    $podObject = @($manifest.objects | Where-Object { $_.name -ceq $podName } | Select-Object -First 1)
                    $podMember = @($cryopodMembers | Where-Object { $_.name -ceq $podName } | Select-Object -First 1)
                    Assert-Equal $podObject.Count 1 "White-model manifest object missing for $podName."
                    Assert-Equal $podMember.Count 1 "White-model measured assembly member missing for $podName."
                    if ($podObject.Count -eq 1) {
                        $podBoundsMin = @($podObject[0].measured_world_bounds_m.min)
                        $podBoundsMax = @($podObject[0].measured_world_bounds_m.max)
                        $podDimensions = @(
                            ([double]$podBoundsMax[0] - [double]$podBoundsMin[0]),
                            ([double]$podBoundsMax[1] - [double]$podBoundsMin[1]),
                            ([double]$podBoundsMax[2] - [double]$podBoundsMin[2])
                        )
                        Assert-LessOrEqual $podDimensions[0] 1.6 0.00001 "$podName measured X dimension"
                        Assert-LessOrEqual $podDimensions[1] 1.6 0.00001 "$podName measured Y dimension"
                        Assert-LessOrEqual $podDimensions[2] 3.0 0.00001 "$podName measured Z dimension"
                        Assert-True ([double]$podBoundsMin[2] -ge -0.00001) "$podName measured minimum Z must be on or above deck."
                    }
                    if ($podMember.Count -eq 1) {
                        Assert-Near ([double]$podMember[0].measured_recline_degrees) 18.0 0.00001 "$podName measured recline"
                        Assert-Equal $podMember[0].leans_toward '+Y' "$podName measured lean direction mismatch."
                    }
                }

                $expectedDockNames = @(
                    'SM_ShipHub_ConstructDock_01', 'SM_ShipHub_ConstructDock_02',
                    'SM_ShipHub_ConstructDock_03', 'SM_ShipHub_ConstructDock_04'
                )
                $expectedDockCenters = @(
                    @(-5.3, -5.3, 0.0), @(5.3, -5.3, 0.0),
                    @(-5.3, 5.3, 0.0), @(5.3, 5.3, 0.0)
                )
                Assert-Equal $manifest.assemblies.construct_docks.count 4 'White-model construct dock count mismatch.'
                Assert-SequenceEqual @($manifest.assemblies.construct_docks.names) $expectedDockNames 'White-model construct dock names'
                $actualDockCenters = @($manifest.assemblies.construct_docks.centers_m)
                Assert-Equal $actualDockCenters.Count $expectedDockCenters.Count 'White-model construct dock center count mismatch.'
                for ($index = 0; $index -lt $expectedDockCenters.Count; $index++) {
                    $actualCenter = if ($index -lt $actualDockCenters.Count) { @($actualDockCenters[$index]) } else { @() }
                    Assert-SequenceEqual $actualCenter $expectedDockCenters[$index] "White-model construct dock center $index"
                }

                Assert-Equal $manifest.assemblies.navigation_table.count 1 'White-model navigation-table assembly count mismatch.'
                Assert-Equal $manifest.assemblies.airlock.count 1 'White-model airlock assembly count mismatch.'

                foreach ($object in @($manifest.objects)) {
                    Assert-SequenceEqual @($object.scale) @(1.0, 1.0, 1.0) "White-model object '$($object.name)' scale"
                }

                Assert-SequenceEqual @($manifest.sheet_ids) $expectedSheets 'White-model sheet IDs'
                Assert-True ([int]$manifest.exports.fbx.object_count -gt 0) 'White-model FBX export object count must be nonzero.'
                Assert-True ([int]$manifest.exports.glb.object_count -gt 0) 'White-model GLB export object count must be nonzero.'
                Assert-Equal $manifest.exports.fbx.object_count $manifest.exports.glb.object_count 'White-model FBX/GLB export object counts must match.'
                $actualFbxNames = @($manifest.exports.fbx.actual_object_names | Where-Object { $null -ne $_ })
                $actualGlbNames = @($manifest.exports.glb.actual_object_names | Where-Object { $null -ne $_ })
                Assert-Equal $actualFbxNames.Count $manifest.exports.fbx.object_count 'White-model FBX actual readback count mismatch.'
                Assert-Equal $actualGlbNames.Count $manifest.exports.glb.object_count 'White-model GLB actual readback count mismatch.'
                Assert-SequenceEqual $actualFbxNames $actualGlbNames 'White-model FBX/GLB actual readback object names'
                Assert-SequenceEqual $actualFbxNames @($actualFbxNames | Sort-Object) 'White-model actual readback object ordering'
                foreach ($object in @($manifest.objects)) {
                    $actualExported = $actualFbxNames -ccontains [string]$object.name
                    Assert-Equal ([bool]$object.exported) $actualExported "White-model object '$($object.name)' exported flag mismatch against actual readback."
                }
            }
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
