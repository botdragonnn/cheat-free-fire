Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Runtime.InteropServices;

public static class VerifyBlob
{
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr FindResource(IntPtr hModule, IntPtr lpName, IntPtr lpType);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr LoadResource(IntPtr hModule, IntPtr hResInfo);

    [DllImport("kernel32.dll")]
    static extern IntPtr LockResource(IntPtr hResData);

    [DllImport("kernel32.dll")]
    static extern uint SizeofResource(IntPtr hModule, IntPtr hResInfo);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool FreeLibrary(IntPtr hModule);

    public static string Check(string exePath, string dllPath)
    {
        IntPtr hMod = LoadLibraryEx(exePath, IntPtr.Zero, 0x00000002); // LOAD_LIBRARY_AS_DATAFILE
        if (hMod == IntPtr.Zero)
            return "LoadLibraryEx falhou";
        try
        {
            IntPtr hRes = FindResource(hMod, (IntPtr)101, (IntPtr)10); // RT_RCDATA = 10
            if (hRes == IntPtr.Zero)
                return "recurso 101 nao encontrado";
            uint size = SizeofResource(hMod, hRes);
            IntPtr p = LockResource(LoadResource(hMod, hRes));
            if (p == IntPtr.Zero)
                return "LockResource falhou";

            byte[] blob = new byte[size];
            Marshal.Copy(p, blob, 0, (int)size);

            uint state = 0x6D2B79F5u;
            for (int i = 0; i < blob.Length; ++i)
            {
                state = state * 1664525u + 1013904223u;
                blob[i] ^= (byte)(state & 0xFF);
            }

            byte[] dll = File.ReadAllBytes(dllPath);
            bool ok = blob.Length == dll.Length;
            if (ok)
            {
                string ha = "", hb = "";
                using (var sha = System.Security.Cryptography.SHA256.Create())
                {
                    ha = BitConverter.ToString(sha.ComputeHash(blob)).Replace("-", "");
                    hb = BitConverter.ToString(sha.ComputeHash(dll)).Replace("-", "");
                }
                ok = ha == hb;
                return "tam=" + blob.Length + " sha256exe=" + ha + " sha256dll=" + hb + " IGUAIS=" + ok;
            }
            return "TAMANHO DIFERENTE: blob=" + blob.Length + " dll=" + dll.Length;
        }
        finally
        {
            FreeLibrary(hMod);
        }
    }
}
"@

[VerifyBlob]::Check("C:\Users\primata\Downloads\ZmInternal\Loader\x64\Release\System.exe", "C:\Users\primata\Downloads\ZmInternal\Loader\HwMonCore.dll")