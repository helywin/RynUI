[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $Title,

    [Parameter(Mandatory)]
    [string] $OutputPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class RynWindowCaptureNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr handle, out Rect rectangle);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr handle);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(
        IntPtr handle,
        IntPtr insertAfter,
        int x,
        int y,
        int width,
        int height,
        uint flags);
}
'@

$process = Get-Process | Where-Object { $_.MainWindowTitle -eq $Title } |
    Select-Object -First 1
if(-not $process) {
    throw "No visible window matched title: $Title"
}

$handle = $process.MainWindowHandle
[void] [RynWindowCaptureNative]::SetWindowPos(
    $handle,
    [IntPtr]::Zero,
    80,
    80,
    0,
    0,
    0x0015)
[RynWindowCaptureNative+Rect] $rectangle = New-Object RynWindowCaptureNative+Rect
if(-not [RynWindowCaptureNative]::GetWindowRect($handle, [ref] $rectangle)) {
    throw 'GetWindowRect failed.'
}

[void] [RynWindowCaptureNative]::SetForegroundWindow($handle)
Start-Sleep -Milliseconds 500

$width = $rectangle.Right - $rectangle.Left
$height = $rectangle.Bottom - $rectangle.Top
if($width -le 0 -or $height -le 0) {
    throw 'The selected window has invalid bounds.'
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutput)
[void] [System.IO.Directory]::CreateDirectory($outputDirectory)

$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen(
        $rectangle.Left,
        $rectangle.Top,
        0,
        0,
        (New-Object System.Drawing.Size $width, $height))
    $bitmap.Save($resolvedOutput, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}

Write-Output $resolvedOutput
