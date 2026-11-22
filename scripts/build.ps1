# build.ps1 - compila usando somente o toolchain portatil dentro do projeto.
# Detalhes Windows:
#  - o MinGW/ld quebra com espacos no proprio caminho de instalacao -> criamos um
#    espelho de hardlinks (mesmos arquivos, sem copiar 900 MB) em um caminho sem espacos;
#  - o diretorio do projeto tem espacos -> usamos o nome curto 8.3 para CMake/Ninja.
$ErrorActionPreference = 'Continue'

function Get-ShortPath {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $Path }
    $short = (& cmd.exe /c ('for %I in ("' + $Path + '") do @echo %~sI')).Trim()
    if ($short -and (Test-Path -LiteralPath $short)) { return $short }
    return $Path
}

function New-HardlinkMirror {
    param([string]$Source, [string]$Destination)
    if (-not (Test-Path -LiteralPath $Source)) { throw "Origem inexistente: $Source" }
    [System.IO.Directory]::CreateDirectory($Destination) | Out-Null
    $prefix = $Source.TrimEnd('\', '/') + '\'
    $items = Get-ChildItem -LiteralPath $Source -Recurse -Force
    foreach ($item in $items) {
        $relative = $item.FullName.Substring($prefix.Length)
        $target = Join-Path $Destination $relative
        if ($item.PSIsContainer) {
            if (-not [System.IO.Directory]::Exists($target)) {
                [System.IO.Directory]::CreateDirectory($target) | Out-Null
            }
        } elseif (-not [System.IO.File]::Exists($target)) {
            $parent = [System.IO.Path]::GetDirectoryName($target)
            if (-not [System.IO.Directory]::Exists($parent)) {
                [System.IO.Directory]::CreateDirectory($parent) | Out-Null
            }
            New-Item -ItemType HardLink -Path $target -Target $item.FullName -Force | Out-Null
        }
    }
}

$projectLong = Split-Path -Parent $PSScriptRoot
$project     = Get-ShortPath $projectLong
$third       = Join-Path $project 'ThirdParty'
$cmake       = Join-Path $third 'cmake\bin\cmake.exe'
$ninja       = Join-Path $third 'ninja\ninja.exe'

foreach ($tool in @($cmake, $ninja)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "Ferramenta ausente: $tool`nExecute primeiro scripts\bootstrap.ps1"
    }
}

$mirror = 'C:\Users\joaog\AppData\Local\Temp\opencode\central-audio-toolchain\mingw64'
if (-not (Test-Path -LiteralPath "$mirror\bin\g++.exe")) {
    Write-Host 'Criando espelho de hardlinks do toolchain (somente uma vez)...'
    New-HardlinkMirror -Source (Join-Path $projectLong 'ThirdParty\mingw64') -Destination $mirror
}

$gcc = Join-Path $mirror 'bin\gcc.exe'
$gpp = Join-Path $mirror 'bin\g++.exe'
foreach ($tool in @($gcc, $gpp)) {
    if (-not (Test-Path -LiteralPath $tool)) { throw "Compilador ausente: $tool" }
}

$env:PATH = "$(Split-Path -Parent $cmake);$(Split-Path -Parent $ninja);$(Split-Path -Parent $gpp);$env:PATH"

$build = Join-Path $project 'build'
& $cmake -S $project -B $build -G Ninja `
    "-DCMAKE_C_COMPILER=$gcc" `
    "-DCMAKE_CXX_COMPILER=$gpp" `
    '-DCMAKE_BUILD_TYPE=Release'
if ($LASTEXITCODE -ne 0) { throw 'Falha na configuracao do CMake' }

& $cmake --build $build --parallel
if ($LASTEXITCODE -ne 0) { throw 'Falha na compilacao' }

$exe = Join-Path $project 'bin\CentralDeAudio.exe'
if (Test-Path -LiteralPath $exe) {
    Write-Host ''
    Write-Host "Pronto: $exe"
}
