# bootstrap.ps1 - prepara TODAS as dependencias dentro da pasta do projeto.
# Nada e instalado no sistema: toolchain, CMake, Ninja e fontes sao apenas descompactados aqui.
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$project = Split-Path -Parent $PSScriptRoot
$third   = Join-Path $project 'ThirdParty'
$cache   = Join-Path $project '.cache'
New-Item -ItemType Directory -Path $third -Force | Out-Null
New-Item -ItemType Directory -Path $cache -Force | Out-Null

function Get-Url {
    param([string]$Url, [string]$Out)
    if (Test-Path -LiteralPath $Out) { Write-Host "  ja existe: $(Split-Path -Leaf $Out)"; return }
    Write-Host "  baixando: $(Split-Path -Leaf $Out)"
    for ($i = 1; $i -le 3; $i++) {
        try {
            Invoke-WebRequest -Uri $Url -OutFile $Out -UseBasicParsing -TimeoutSec 1200
            if ((Get-Item -LiteralPath $Out).Length -lt 1000) { throw 'arquivo suspeitosamente pequeno' }
            return
        } catch {
            Write-Warning "  tentativa $i falhou: $($_.Exception.Message)"
            if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Force }
            if ($i -eq 3) { throw "Falha ao baixar $Url" }
        }
    }
}

function Expand-ZipTo {
    param([string]$Zip, [string]$Dest)
    if (-not (Test-Path -LiteralPath $Dest)) { New-Item -ItemType Directory -Path $Dest -Force | Out-Null }
    $tmp = Join-Path $Dest '__unpack__'
    if (Test-Path -LiteralPath $tmp) { Remove-Item -Recurse -Force -LiteralPath $tmp }
    New-Item -ItemType Directory -Path $tmp | Out-Null
    & tar.exe -xf $Zip -C $tmp
    if ($LASTEXITCODE -ne 0) { throw "Falha ao extrair $Zip" }
    $items = @(Get-ChildItem -Force -LiteralPath $tmp)
    if ($items.Count -eq 1 -and $items[0].PSIsContainer) {
        $source = $items[0].FullName
    } else {
        $source = $tmp
    }
    Get-ChildItem -Force -LiteralPath $source | ForEach-Object {
        Move-Item -Force -LiteralPath $_.FullName -Destination $Dest
    }
    if ($source -ne $tmp -and (Test-Path -LiteralPath $source)) { Remove-Item -Recurse -Force -LiteralPath $source }
    Remove-Item -Recurse -Force -LiteralPath $tmp
}

# --- URLs (versoes fixadas para o build ser reproduzivel) ---
$urlMinGW = 'https://github.com/brechtsanders/winlibs_mingw/releases/download/16.2.0posix-23.1.3-14.0.0-ucrt-r3/winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3.zip'
$urlCmake = 'https://github.com/Kitware/CMake/releases/download/v4.5.1/cmake-4.5.1-windows-x86_64.zip'
$urlNinja = 'https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip'
$urlImGui = 'https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0.zip'
$urlAsio  = 'https://github.com/audiosdk/asio/archive/refs/heads/main.zip'

Write-Host '=== [1/5] MinGW-w64 (compilador portatil) ==='
$zip = Join-Path $cache 'mingw.zip'
Get-Url $urlMinGW $zip
if (-not (Test-Path -LiteralPath (Join-Path $third 'mingw64\bin\g++.exe'))) {
    Expand-ZipTo $zip (Join-Path $third 'mingw64')
}

Write-Host '=== [2/5] CMake ==='
$zip = Join-Path $cache 'cmake.zip'
Get-Url $urlCmake $zip
if (-not (Test-Path -LiteralPath (Join-Path $third 'cmake\bin\cmake.exe'))) {
    Expand-ZipTo $zip (Join-Path $third 'cmake')
}

Write-Host '=== [3/5] Ninja ==='
$zip = Join-Path $cache 'ninja.zip'
Get-Url $urlNinja $zip
if (-not (Test-Path -LiteralPath (Join-Path $third 'ninja\ninja.exe'))) {
    Expand-ZipTo $zip (Join-Path $third 'ninja')
}

Write-Host '=== [4/5] Dear ImGui ==='
$zip = Join-Path $cache 'imgui.zip'
Get-Url $urlImGui $zip
if (-not (Test-Path -LiteralPath (Join-Path $third 'imgui\imgui.h'))) {
    Expand-ZipTo $zip (Join-Path $third 'imgui')
}

Write-Host '=== [5/5] Steinberg ASIO SDK ==='
$zip = Join-Path $cache 'asio_sdk.zip'
Get-Url $urlAsio $zip
if (-not (Test-Path -LiteralPath (Join-Path $third 'asio_sdk\asio\asio.h'))) {
    Expand-ZipTo $zip (Join-Path $third 'asio_sdk')
}

Write-Host ''
Write-Host 'Dependencias prontas em:' $third
Write-Host '  mingw64\bin\g++.exe'
Write-Host '  cmake\bin\cmake.exe'
Write-Host '  ninja\ninja.exe'
Write-Host '  imgui\'
Write-Host '  asio_sdk\'
