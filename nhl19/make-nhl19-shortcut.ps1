# SPDX-FileCopyrightText: 2026 griggsbd
# SPDX-License-Identifier: GPL-2.0-or-later

# Creates an "NHL 19" desktop shortcut that starts the game with this shadPS4 build.
# Edit the two paths below, then run:
#   powershell -ExecutionPolicy Bypass -File .\make-nhl19-shortcut.ps1
# To change paths later, edit them and run it again. It replaces the old shortcut.

$ErrorActionPreference = 'Stop'

# Where you put this build's shadPS4.exe.
$exe  = 'C:\Games\shadPS4-NHL19\shadPS4.exe'
# Your NHL 19 folder, the one that contains eboot.bin and sce_sys.
$game = 'C:\Games\PS4\CUSA11126'

$name   = 'NHL 19'
$eboot  = Join-Path $game 'eboot.bin'
$exeDir = Split-Path $exe -Parent

foreach ($path in @($exe, $eboot)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Not found: $path. Check the paths at the top of this script."
    }
}

# shadPS4 keeps its settings in a 'user' folder next to where it starts, if there is one, and in
# %APPDATA%\shadPS4 otherwise. The shortcut starts it from its own folder.
if (Test-Path -LiteralPath (Join-Path $exeDir 'user')) {
    Write-Host "Note: shadPS4 will use the 'user' folder in $exeDir for its settings (portable mode)."
} else {
    Write-Host "Note: shadPS4 will use $env:APPDATA\shadPS4 for its settings."
}

# Turn the game's own icon (sce_sys\icon0.png) into a 256x256 Windows .ico next to the exe.
$iconPng = Join-Path $game 'sce_sys\icon0.png'
$iconIco = Join-Path $exeDir 'NHL19.ico'
$iconLocation = "$exe,0"

if (Test-Path -LiteralPath $iconPng) {
    Add-Type -AssemblyName System.Drawing
    $source = [System.Drawing.Image]::FromFile($iconPng)
    $bitmap = New-Object System.Drawing.Bitmap 256, 256
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.DrawImage($source, 0, 0, 256, 256)
    $stream = New-Object System.IO.MemoryStream
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytes = $stream.ToArray()
    $graphics.Dispose()
    $bitmap.Dispose()
    $source.Dispose()
    $stream.Dispose()

    # .ico file with a single PNG image: 6-byte header, 16-byte entry, then the PNG data.
    $file = [System.IO.File]::Create($iconIco)
    $writer = New-Object System.IO.BinaryWriter $file
    $writer.Write([UInt16]0)        # reserved
    $writer.Write([UInt16]1)        # type: icon
    $writer.Write([UInt16]1)        # number of images
    $writer.Write([Byte]0)          # width 256 (stored as 0)
    $writer.Write([Byte]0)          # height 256 (stored as 0)
    $writer.Write([Byte]0)          # palette size
    $writer.Write([Byte]0)          # reserved
    $writer.Write([UInt16]1)        # color planes
    $writer.Write([UInt16]32)       # bits per pixel
    $writer.Write([UInt32]$pngBytes.Length)
    $writer.Write([UInt32]22)       # offset of the image data
    $writer.Write($pngBytes)
    $writer.Close()

    $iconLocation = "$iconIco,0"
} else {
    Write-Warning "Game icon not found at $iconPng. Using the shadPS4 icon instead."
}

$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop "$name.lnk"

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $exe
$shortcut.Arguments = "--game `"$eboot`""
$shortcut.WorkingDirectory = $exeDir
$shortcut.IconLocation = $iconLocation
$shortcut.Description = "$name (shadPS4)"
$shortcut.Save()

Write-Host "Created $shortcutPath"
