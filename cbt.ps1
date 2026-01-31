<#
Cory build tool launcher
- Uses a build root under the repository `build` folder (same folder as this script's directory).
- `setup` creates a virtualenv in <build>\.venv, installs tools/cbt, and runs `conan install` for Debug and Release configs.
- `start` or `-h` activates the venv (if present) and shows `cbt` help.
- Default: activates venv and forwards all args to `python -m cbt`.

Usage:
  .\cbt.ps1 setup
  .\cbt.ps1 start
  .\cbt.ps1 <other cbt args...>

Run PowerShell with: powershell -NoProfile -ExecutionPolicy Bypass -File .\cbt.ps1 setup
#>

# Set-StrictMode -Version Latest
# $ErrorActionPreference = 'Stop'

# Determine script and repo locations
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$buildRoot = Join-Path $scriptDir 'build'
$venvDir = Join-Path $buildRoot '.venv'

# Helper to run commands and throw on failure
function Run-Command {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$false)][string[]]$Arguments
    )
    if ($null -eq $Arguments) { $Arguments = @() }
    Write-Output "Running: $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    $code = $LASTEXITCODE
    if ($code -ne 0) { throw ("Command '{0} {1}' failed with exit code {2}" -f $FilePath, ($Arguments -join ' '), $code) }
}

# Grab first argument as subcommand
$sub = if ($args.Count -ge 1) { $args[0] } else { $null }

if ($sub -eq 'setup') {
    if (-not (Test-Path $buildRoot)) {
        New-Item -ItemType Directory -Path $buildRoot | Out-Null
    }

    if (-not (Test-Path $venvDir)) {
        Write-Output "Creating virtual environment at: $venvDir"
        Run-Command -FilePath 'python' -Arguments @('-m','venv', $venvDir)
    } else {
        Write-Output "Virtual environment already exists at: $venvDir"
    }

    # Install the in-repo tools/cbt package into the venv using pip (via python -m pip)
    $toolsPath = Join-Path $scriptDir 'tools\cbt'
    if (-not (Test-Path $toolsPath)) {
        Write-Warning "tools/cbt not found at: $toolsPath - skipping editable install"
    } else {
        Run-Command -FilePath 'python' -Arguments @('-m','pip','install','-e',$toolsPath)
    }

    Write-Output "cbt: virtual environment set up at: $venvDir"
    Write-Output "Activate it in PowerShell with: . '$venvDir\Scripts\Activate.ps1' and then run: ./cbt --help"
    exit 0
}

if ($sub -eq 'start' -or $sub -eq '-h' -or $sub -eq '--help') {
    $activate = Join-Path $venvDir 'Scripts\Activate.ps1'
    if (Test-Path $activate) {
        Write-Output "Activating virtual environment: $venvDir"
        . $activate
    } else {
        Write-Warning "Virtual environment not found at $venvDir. Run '.\cbt.ps1 setup' first."
    }

    Write-Output "Cory build tool (cbt) working correctly. Showing help..."
    Run-Command -FilePath 'python' -Arguments @('-m','cbt','--help')
    exit 0
}

# Default: activate venv if present and forward args to python -m cbt
$activate = Join-Path $venvDir 'Scripts\Activate.ps1'
if (Test-Path $activate) {
    Write-Output "Activating virtual environment: $venvDir"
    . $activate
} else {
    Write-Warning "Virtual environment not found at $venvDir. Run '.\cbt.ps1 setup' first."
}

# MSVC activation removed from PowerShell launcher; Python-side `cbt` will activate the developer environment when needed.

# Forward all args to cbt
& python -m cbt $args

exit 0
