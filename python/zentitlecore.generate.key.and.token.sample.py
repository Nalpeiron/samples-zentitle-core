#!/usr/bin/env python3
import os, sys, json, ctypes, platform, base64, datetime
from ctypes import c_int, c_int32, c_char, c_char_p, c_uint8, POINTER, create_string_buffer, byref

# --- Auto-detect Core library name ---
system = platform.system()
if system == "Windows":
    LIB_NAME = "Zentitle2Core.dll"
elif system == "Darwin":
    LIB_NAME = "libZentitle2Core.dylib"
else:
    LIB_NAME = "libZentitle2Core.so"

HERE = os.path.dirname(os.path.abspath(__file__))
LIB_PATH = os.path.join(HERE, LIB_NAME)
if not os.path.exists(LIB_PATH):
    print(f"❌ Library not found: {LIB_PATH}")
    sys.exit(2)

# Load lib (help resolve transitive deps on *nix)
try:
    load_mode = ctypes.RTLD_GLOBAL
except AttributeError:
    load_mode = None
if system == "Windows":
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(HERE)
    lib = ctypes.CDLL(LIB_PATH)
else:
    lib = ctypes.CDLL(LIB_PATH, mode=load_mode) if load_mode else ctypes.CDLL(LIB_PATH)

# --- Function signatures (per Core header) ---
lib.enableLogging.argtypes = (c_char_p,)
lib.enableLogging.restype = None

lib.generateDefaultEncryptionKey.argtypes = (POINTER(c_uint8),)
lib.generateDefaultEncryptionKey.restype = c_int32

lib.generateActivationRequestToken.argtypes = (
    POINTER(c_char), POINTER(c_int), c_char_p, c_char_p, c_char_p
)
lib.generateActivationRequestToken.restype = c_int32

lib.getVersion.argtypes = (POINTER(c_char), POINTER(c_int))
lib.getVersion.restype = c_int32

# --- Constants ---
# Replace the placeholder values below with your real product details and public key.

PUBLIC_KEY_PEM = b"""-----BEGIN PUBLIC KEY-----
<INSERT YOUR RSA PUBLIC KEY HERE>
-----END PUBLIC KEY-----
"""

PRODUCT_ID = "<INSERT PRODUCT ID HERE>"          # e.g., "prod_N-22qg3B90mcaEr92bFYvA"
ACTIVATION_CODE = "<INSERT ACTIVATION CODE>"     # e.g., "ES2H-5265-7F4L-GXNC"
TOKEN_ISSUE_DATE = "<INSERT ISO DATE>"           # e.g., "2025-03-10T04:28:11.3502447+00:00"
REQUEST_ID = "<INSERT UNIQUE REQUEST ID>"        # e.g., "d4f7327d-7104-4cd3-a9b2-da611dc72b0e"

# Offline deactivation requires the activation identifier returned in the activation response payload.
ACTIVATION_ID = "<INSERT ACTIVATION ID>"         # e.g., "act_01HZYAC4XMVFVGKJ4ZX0S3MY0K"
# Optional: override the auto-generated timestamp for the deactivation token.
DEACTIVATION_TOKEN_ISSUE_DATE = ""               # leave blank to use current UTC time

BUFFER_SIZE = 100_000
ACTIVATION_AES_FILE = "aes_key_generated.base64.txt"
DEACTIVATION_AES_FILE = "aes_key_deactivation.base64.txt"
FORCE_NEW_ACTIVATION_AES = False

def seat_id() -> str:
    base = "ZEN_WIN64" if system == "Windows" else f"ZEN_{system.upper()}"
    return base + "-offline"

def iso8601_utc_now() -> str:
    now = datetime.datetime.utcnow()
    return f"{now:%Y-%m-%dT%H:%M:%S}.{now.microsecond:06d}0+00:00"

def generate_aes_key_base64() -> str:
    key_raw = (c_uint8 * 32)()
    status = lib.generateDefaultEncryptionKey(key_raw)
    if status != 0:
        print(f"❌ generateDefaultEncryptionKey() failed with status code {status}")
        sys.exit(1)
    key_bytes = bytes(bytearray(key_raw))
    return base64.b64encode(key_bytes).decode("ascii")

def load_saved_aes_key(filename: str) -> str:
    path = os.path.join(HERE, filename)
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read().strip()
    except FileNotFoundError:
        return ""

def call_generate_activation_request_token(payload: dict, aes_key_b64: str, tag: str) -> str:
    payload_json = json.dumps(payload, separators=(",", ":")).encode("utf-8")

    out = create_string_buffer(BUFFER_SIZE)
    n = c_int(BUFFER_SIZE)
    status = lib.generateActivationRequestToken(
        out,
        byref(n),
        payload_json,
        aes_key_b64.encode("ascii"),
        PUBLIC_KEY_PEM
    )

    print(f"{tag} token status: {status} | length: {n.value}")

    if status != 0 or n.value <= 3:
        print(f"❌ {tag} FAILED with status code {status}. Check the RSA public key and payload.")
        sys.exit(1)

    return out.value.decode("utf-8", "replace")

def main():
    print("=== Zentitle2Core: Generate AES key -> Generate token ===")
    print("Library:", LIB_PATH)

    lib.enableLogging(None)

    try:
        buf = create_string_buffer(1024)
        n = c_int(1024)
        if lib.getVersion(buf, byref(n)) == 0:
            print("Version:", buf.value.decode("utf-8", "replace"))
    except Exception:
        pass

    activation_aes = ""
    activation_token = ""
    generated_activation = False

    activation_inputs_ready = all(
        value and "<" not in value
        for value in (PRODUCT_ID, ACTIVATION_CODE, REQUEST_ID)
    )

    if activation_inputs_ready:
        reused_activation_key = False
        if not FORCE_NEW_ACTIVATION_AES:
            saved_key = load_saved_aes_key(ACTIVATION_AES_FILE)
            if saved_key:
                activation_aes = saved_key
                reused_activation_key = True

        if not activation_aes:
            activation_aes = generate_aes_key_base64()

        if reused_activation_key:
            print("Activation AES key (Base64):", activation_aes, "(reused)")
        else:
            print("Activation AES key (Base64):", activation_aes)

        activation_payload = {
            "productId": PRODUCT_ID,
            "activationCode": ACTIVATION_CODE,
            "seatId": seat_id(),
            "tokenIssueDate": TOKEN_ISSUE_DATE if TOKEN_ISSUE_DATE and "<" not in TOKEN_ISSUE_DATE else iso8601_utc_now(),
            "requestId": REQUEST_ID,
        }

        activation_token = call_generate_activation_request_token(activation_payload, activation_aes, "Activation")
        print("\n✅ SUCCESS – activation token generated.")
        print("Preview:", activation_token[:100] + ("..." if len(activation_token) > 100 else ""))

        with open(os.path.join(HERE, ACTIVATION_AES_FILE), "w", encoding="utf-8") as f:
            f.write(activation_aes)
        with open(os.path.join(HERE, "offline_request_token.txt"), "w", encoding="utf-8") as f:
            f.write(activation_token)
        generated_activation = True
    else:
        activation_aes = load_saved_aes_key(ACTIVATION_AES_FILE)
        if activation_aes:
            print("ℹ️ Activation inputs missing. Loaded AES key from file for reuse.")
        else:
            print("ℹ️ Activation inputs missing and no saved AES key found. Activation token generation skipped.")

    actual_activation_id = ACTIVATION_ID if ACTIVATION_ID and "<" not in ACTIVATION_ID else ""
    if actual_activation_id:
        deactivation_issue_date = (
            DEACTIVATION_TOKEN_ISSUE_DATE
            if DEACTIVATION_TOKEN_ISSUE_DATE
            else iso8601_utc_now()
        )

        deactivation_payload = {
            "ActivationId": actual_activation_id,
            "TokenIssueDate": deactivation_issue_date,
        }

        deactivation_aes = activation_aes
        if not deactivation_aes:
            print("❌ Missing activation AES key; cannot build deactivation token.")
            sys.exit(1)

        print("Deactivation AES key (Base64):", deactivation_aes, "(reused from activation)")

        deactivation_token = call_generate_activation_request_token(deactivation_payload, deactivation_aes, "Deactivation")
        print("\n✅ SUCCESS – deactivation token generated.")
        print("Deactivation preview:", deactivation_token[:100] + ("..." if len(deactivation_token) > 100 else ""))

        with open(os.path.join(HERE, DEACTIVATION_AES_FILE), "w", encoding="utf-8") as f:
            f.write(deactivation_aes)
        with open(os.path.join(HERE, "offline_deactivation_request_token.txt"), "w", encoding="utf-8") as f:
            f.write(deactivation_token)
    else:
        print("\nℹ️ Skipping offline deactivation token generation. Set ACTIVATION_ID once you have it from the activation response.")

    print("\nSaved:")
    if generated_activation:
        print(f" - {ACTIVATION_AES_FILE}")
        print(" - offline_request_token.txt")
    elif activation_aes:
        print(f" - {ACTIVATION_AES_FILE} (existing)")
    if actual_activation_id:
        print(f" - {DEACTIVATION_AES_FILE}")
        print(" - offline_deactivation_request_token.txt")

if __name__ == "__main__":
    # macOS: export DYLD_LIBRARY_PATH=$(pwd):$DYLD_LIBRARY_PATH
    # Linux: export LD_LIBRARY_PATH=$(pwd):$LD_LIBRARY_PATH
    # Windows: run with `python generate_key_and_token.py`
    main()
