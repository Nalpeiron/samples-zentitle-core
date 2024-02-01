from ctypes import *
import sys  # Import missing module

def load_dynamic_library():
    # Use a raw string or replace \ with \\ for the path
    library_path = r"C:\Sample\Path\"  # Define this your library path
    library_name = {
        'win32': 'Zentitle2Core.dll',
        'darwin': 'libZentitle2Core.dylib',
        'linux': 'libZentitle2Core.so'
    }.get(sys.platform, 'Zentitle2Core.dll')  # Default to DLL if platform not found
    lib_full_name = library_path + library_name
    return cdll.LoadLibrary(lib_full_name)

lib = load_dynamic_library()

# Assuming the function signature is known (e.g., returns bool and takes char* as argument)
generateDefaultDeviceFingerprint = lib.generateDefaultDeviceFingerprint
generateDefaultDeviceFingerprint.restype = c_bool
generateDefaultDeviceFingerprint.argtypes = [POINTER(c_char)]

device_fingerprint = create_string_buffer(100)  # Buffer to store the fingerprint
result = generateDefaultDeviceFingerprint(device_fingerprint)

print("Function called successfully:", result)
# Print the device fingerprint. Assuming it's a UTF-8 encoded string.
print("Device Fingerprint:", device_fingerprint.value.decode('utf-8'))
