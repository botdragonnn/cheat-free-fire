# Encrypt do modulo embutido (HwMonCore.dll -> HwMonCore.bin).
# XOR com keystream LCG 32-bit (multiply-Add mod 2^32) — MESMA sequencia
# do DecryptDllBlob() em Loader/main.cpp. Roda no PreBuildEvent do vcxproj
# (a pasta do script = pasta do projeto). O loop roda em C# inline
# (Add-Type) — instantes, em vez de minutos no interpretador PS.

$ErrorActionPreference = "SilentlyContinue"

$src = Join-Path $PSScriptRoot "HwMonCore.dll"
$dst = Join-Path $PSScriptRoot "HwMonCore.bin"

if (-not (Test-Path -LiteralPath $src)) {
    Write-Output "encrypt_dll: HwMonCore.dll nao encontrada em $PSScriptRoot"
    exit 0
}

Add-Type -TypeDefinition @"
using System;
using System.IO;

public static class BlobCipher
{
    public static void EncryptFile(string src, string dst)
    {
        byte[] data = File.ReadAllBytes(src);
        uint state = 0x6D2B79F5u;
        for (int i = 0; i < data.Length; ++i)
        {
            state = state * 1664525u + 1013904223u;
            data[i] ^= (byte)(state & 0xFF);
        }
        File.WriteAllBytes(dst, data);
    }
}
"@

[BlobCipher]::EncryptFile($src, $dst)
$size = (Get-Item -LiteralPath $dst).Length
Write-Output "encrypt_dll: $size bytes cifrados -> HwMonCore.bin"