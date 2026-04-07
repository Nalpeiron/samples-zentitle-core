using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

internal static class Program
{
  private const int FingerprintBufferSize = 128;

#if WINDOWS
  private const string LibraryName = "Zentitle2Core.dll";
#elif OSX
  private const string LibraryName = "libZentitle2Core.dylib";
#else
  private const string LibraryName = "libZentitle2Core.so";
#endif

  [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
  private static extern bool generateDefaultDeviceFingerprint(byte[] fingerprint, ref int length);

  private static int Main(string[] args)
  {
    var resolvedLibraryPath = args.Length > 0
      ? args[0]
      : Path.Combine(AppContext.BaseDirectory, LibraryName);

    Console.WriteLine($"Expected library path: {resolvedLibraryPath}");
    Console.WriteLine("Ensure the runtime library is available via the executable directory or the OS loader path.");

    var buffer = new byte[FingerprintBufferSize];
    var fingerprintLength = 0;

    var result = generateDefaultDeviceFingerprint(buffer, ref fingerprintLength);
    if (!result)
    {
      Console.Error.WriteLine("generateDefaultDeviceFingerprint returned false");
      return 1;
    }

    if (fingerprintLength < 0 || fingerprintLength >= buffer.Length)
    {
      Console.Error.WriteLine($"Invalid fingerprint length returned by library: {fingerprintLength}");
      return 1;
    }

    var fingerprint = Encoding.UTF8.GetString(buffer, 0, fingerprintLength);
    Console.WriteLine($"Device fingerprint: {fingerprint}");
    return 0;
  }
}
