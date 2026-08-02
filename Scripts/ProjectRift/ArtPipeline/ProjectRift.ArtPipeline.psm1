Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-ProjectRiftContainedArtPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Candidate,
        [Parameter(Mandatory)][string]$AllowedRoot
    )

    try {
        $normalizedCandidate = [IO.Path]::GetFullPath($Candidate)
        $normalizedRoot = [IO.Path]::GetFullPath($AllowedRoot)
        $separator = [IO.Path]::DirectorySeparatorChar
        $alternateSeparator = [IO.Path]::AltDirectorySeparatorChar

        if ($normalizedCandidate.Equals($normalizedRoot, [StringComparison]::OrdinalIgnoreCase)) {
            return $false
        }

        $rootPrefix = $normalizedRoot.TrimEnd($separator, $alternateSeparator) + $separator
        return $normalizedCandidate.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)
    }
    catch {
        return $false
    }
}

function Resolve-ProjectRiftBlenderExecutable {
    [CmdletBinding()]
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        try {
            $fullExplicitPath = [IO.Path]::GetFullPath($ExplicitPath)
        }
        catch {
            throw "Explicit Blender path is invalid: $ExplicitPath"
        }
        if ((Test-Path -LiteralPath $fullExplicitPath -PathType Leaf) -and ([IO.Path]::GetFileName($fullExplicitPath) -ieq 'blender.exe')) {
            return $fullExplicitPath
        }
        throw "Explicit Blender path must name an existing blender.exe: $fullExplicitPath"
    }

    $candidates = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($env:PROJECTRIFT_BLENDER_EXE)) {
        $candidates.Add($env:PROJECTRIFT_BLENDER_EXE)
    }

    $blenderCommand = Get-Command blender -ErrorAction SilentlyContinue
    if ($null -ne $blenderCommand -and -not [string]::IsNullOrWhiteSpace($blenderCommand.Path)) {
        $candidates.Add($blenderCommand.Path)
    }

    foreach ($candidate in $candidates) {
        try {
            $fullPath = [IO.Path]::GetFullPath($candidate)
            if ((Test-Path -LiteralPath $fullPath -PathType Leaf) -and ([IO.Path]::GetFileName($fullPath) -ieq 'blender.exe')) {
                return $fullPath
            }
        }
        catch {
            continue
        }
    }

    throw 'Blender 5.2 LTS is unavailable. Pass -BlenderExe or set PROJECTRIFT_BLENDER_EXE.'
}

function Resolve-ProjectRiftPythonExecutable {
    [CmdletBinding()]
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        try {
            $fullExplicitPath = [IO.Path]::GetFullPath($ExplicitPath)
        }
        catch {
            throw "Explicit Python path is invalid: $ExplicitPath"
        }
        if (Test-Path -LiteralPath $fullExplicitPath -PathType Leaf) {
            return $fullExplicitPath
        }
        throw "Explicit Python path must name an existing executable: $fullExplicitPath"
    }

    $candidates = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($env:PROJECTRIFT_PYTHON_EXE)) {
        $candidates.Add($env:PROJECTRIFT_PYTHON_EXE)
    }

    foreach ($candidate in $candidates) {
        try {
            $fullPath = [IO.Path]::GetFullPath($candidate)
            if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
                return $fullPath
            }
        }
        catch {
            continue
        }
    }

    throw 'A Python executable with Pillow, ReportLab and pypdf is required. Pass -PythonExe or set PROJECTRIFT_PYTHON_EXE.'
}

Export-ModuleMember -Function Test-ProjectRiftContainedArtPath, Resolve-ProjectRiftBlenderExecutable, Resolve-ProjectRiftPythonExecutable
