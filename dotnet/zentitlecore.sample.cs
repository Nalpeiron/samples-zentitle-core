using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

internal static class Program
{
  private const int MaxFingerprintLength = 100;
  private const int FingerprintBufferSize = MaxFingerprintLength + 1;

#if WINDOWS
  private const string LibraryName = "Zentitle2Core.dll";
#elif OSX
  private const string LibraryName = "libZentitle2Core.dylib";
#else
  private const string LibraryName = "libZentitle2Core.so";
#endif

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  private delegate bool GenerateDefaultDeviceFingerprintDelegate(byte[] fingerprint, ref int length);

  private static int Main(string[] args)
  {
    var resolvedLibraryPath = ResolveLibraryPath(args);

    if (!File.Exists(resolvedLibraryPath))
    {
      Console.Error.WriteLine($"Library not found: {resolvedLibraryPath}");
      Console.Error.WriteLine("Place the runtime library next to the executable or pass its full path as the first argument.");
      return 1;
    }

    nint libraryHandle;
    try
    {
      libraryHandle = NativeLibrary.Load(resolvedLibraryPath);
    }
    catch (Exception ex) when (ex is DllNotFoundException || ex is BadImageFormatException)
    {
      Console.Error.WriteLine($"Failed to load library: {resolvedLibraryPath}");
      Console.Error.WriteLine(ex.Message);
      return 1;
    }

    GenerateDefaultDeviceFingerprintDelegate generateDefaultDeviceFingerprint;
    try
    {
      var exportHandle = NativeLibrary.GetExport(libraryHandle, "generateDefaultDeviceFingerprint");
      generateDefaultDeviceFingerprint =
        Marshal.GetDelegateForFunctionPointer<GenerateDefaultDeviceFingerprintDelegate>(exportHandle);
    }
    catch (Exception ex) when (ex is EntryPointNotFoundException || ex is MarshalDirectiveException)
    {
      Console.Error.WriteLine("Failed to load symbol: generateDefaultDeviceFingerprint");
      Console.Error.WriteLine(ex.Message);
      NativeLibrary.Free(libraryHandle);
      return 1;
    }

    var buffer = new byte[FingerprintBufferSize];
    var fingerprintLength = MaxFingerprintLength;

    try
    {
      var result = generateDefaultDeviceFingerprint(buffer, ref fingerprintLength);
      if (!result)
      {
        Console.Error.WriteLine("generateDefaultDeviceFingerprint returned false");
        return 1;
      }

      if (fingerprintLength < 0 || fingerprintLength > MaxFingerprintLength)
      {
        Console.Error.WriteLine($"Invalid fingerprint length returned by library: {fingerprintLength}");
        return 1;
      }

      if (buffer[fingerprintLength] != 0)
      {
        Console.Error.WriteLine($"Fingerprint buffer is missing a null terminator at index {fingerprintLength}");
        return 1;
      }

      var fingerprint = Encoding.UTF8.GetString(buffer, 0, fingerprintLength);
      Console.WriteLine($"Loaded library: {resolvedLibraryPath}");
      Console.WriteLine($"Device fingerprint: {fingerprint}");
      return 0;
    }
    finally
    {
      NativeLibrary.Free(libraryHandle);
    }
  }

  private static string ResolveLibraryPath(string[] args)
  {
    if (args.Length > 0)
    {
      return args[0];
    }

    return Path.Combine(AppContext.BaseDirectory, LibraryName);
  }
}
