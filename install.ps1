$ErrorActionPreference = "Stop"

function Stop-Install([string] $Message) {
    Write-Error "install.ps1: $Message"
    exit 1
}

function Install-WingetPackage([string] $Id, [string] $Override = "") {
    $arguments = @(
        "install", "--id", $Id, "--exact",
        "--accept-package-agreements", "--accept-source-agreements"
    )
    if ($Override) {
        $arguments += @("--override", $Override)
    }
    & winget @arguments
    if ($LASTEXITCODE -ne 0) {
        Stop-Install "no se pudo instalar $Id con winget (código $LASTEXITCODE)."
    }
}

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Stop-Install "instale App Installer/winget desde Microsoft Store y vuelva a ejecutar este script."
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products "*" `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                  Microsoft.VisualStudio.Component.VC.Llvm.Clang `
        -property installationPath
}
if (-not $vsPath) {
    Install-WingetPackage "Microsoft.VisualStudio.2022.BuildTools" `
        "--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --includeRecommended"
}

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Install-WingetPackage "Kitware.CMake"
}

$llvmRoot = Join-Path $env:ProgramFiles "LLVM"
$llvmConfig = Join-Path $llvmRoot "lib\cmake\llvm\LLVMConfig.cmake"
if (-not (Test-Path $llvmConfig)) {
    Install-WingetPackage "LLVM.LLVM"
}

$env:PATH = "$llvmRoot\bin;$env:ProgramFiles\CMake\bin;$env:PATH"
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Stop-Install "CMake no está disponible en PATH; cierre y vuelva a abrir PowerShell después de instalarlo."
}
if (-not (Test-Path $llvmConfig)) {
    Stop-Install "no se encontró LLVMConfig.cmake en $llvmRoot; compruebe la instalación de LLVM."
}

$sourceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $sourceDir "build\windows"
$prefix = Join-Path $env:LOCALAPPDATA "Programs\Rivo"
$architecture = if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "ARM64" } else { "x64" }

& cmake -S $sourceDir -B $buildDir -G "Visual Studio 17 2022" `
    -A $architecture -T ClangCL "-DLLVM_DIR=$(Split-Path -Parent $llvmConfig)"
if ($LASTEXITCODE -ne 0) {
    Stop-Install "CMake no pudo configurar el proyecto."
}
& cmake --build $buildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    Stop-Install "falló la compilación de Rivo."
}
& cmake --install $buildDir --config Release --prefix $prefix
if ($LASTEXITCODE -ne 0) {
    Stop-Install "falló la instalación de Rivo en $prefix."
}

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathEntries = @($userPath -split ";" | Where-Object { $_ })
foreach ($entry in @("$prefix\bin", "$llvmRoot\bin")) {
    if ($pathEntries -notcontains $entry) {
        $pathEntries += $entry
    }
}
[Environment]::SetEnvironmentVariable("Path", ($pathEntries -join ";"), "User")
$env:PATH = "$prefix\bin;$llvmRoot\bin;$env:PATH"

& (Join-Path $prefix "bin\rivo.exe") --version
if ($LASTEXITCODE -ne 0) {
    Stop-Install "Rivo se instaló, pero no se pudo ejecutar para verificar la instalación."
}

Write-Host ""
Write-Host "Rivo se instaló en $prefix"
Write-Host "Cierre y vuelva a abrir PowerShell para usar 'rivo' desde cualquier carpeta."
