using System;
using System.Runtime.InteropServices;
using System.Text;

class Program
{
  [DllImport("Zentitle2Core.dll", CallingConvention = CallingConvention.Cdecl)]
  private static extern bool generateDefaultDeviceFingerprint(IntPtr input);

  static void Main(string[] args)
  {

    IntPtr buffer = Marshal.AllocHGlobal(100); // Allocating memory for the string plus an additional byte for '\0'

    // Invoking the function
    bool result = generateDefaultDeviceFingerprint(buffer);

    // Converting the result back to a string in C#
    string resultString = Marshal.PtrToStringAnsi(buffer);
    Console.WriteLine($"Result: {resultString}");

    // Freeing the allocated memory
    Marshal.FreeHGlobal(buffer);
  }
}
