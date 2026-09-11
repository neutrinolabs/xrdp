#!/usr/bin/env python3
"""
Extended VNC test client with diagnostics for macOS Screen Sharing
"""
import socket, hashlib, struct, sys, getpass, os
from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes

def test_vnc(username=None, password=None, host='127.0.0.1', port=5900):
    print("=" * 60)
    print("VNC Connection Test with Extended Diagnostics")
    print("=" * 60)

    # Get credentials if not provided
    if username is None:
        username = os.environ.get('VNC_USERNAME') or input("Username: ")
    if password is None:
        password = os.environ.get('VNC_PASSWORD') or getpass.getpass("Password: ")

    sock = socket.socket()
    sock.settimeout(10)

    print(f"\n[1] Connecting to {host}:{port}...")
    try:
        sock.connect((host, port))
        print("    Connected!")
    except Exception as e:
        print(f"    FAILED: {e}")
        return

    # Protocol version
    server_version = sock.recv(12)
    print(f"\n[2] Server version: {server_version.decode().strip()}")
    sock.send(b'RFB 003.008\n')
    print("    Sent client version: RFB 003.008")

    # Security types
    num = sock.recv(1)[0]
    sec_types = sock.recv(num)
    print(f"\n[3] Security types offered: {list(sec_types)}")
    print("    (30 = Apple ARD, 2 = VNC Auth, 1 = None)")

    if 30 not in sec_types:
        print("    WARNING: ARD (30) not offered!")
        sock.close()
        return

    # Select ARD auth
    sock.send(bytes([30]))
    print("    Selected: 30 (Apple ARD)")

    # DH parameters
    gen = int.from_bytes(sock.recv(2), 'big')
    klen = int.from_bytes(sock.recv(2), 'big')
    prime = int.from_bytes(sock.recv(klen), 'big')
    spub = int.from_bytes(sock.recv(klen), 'big')
    print(f"\n[4] DH Parameters received:")
    print(f"    Generator: {gen}")
    print(f"    Key length: {klen} bytes ({klen*8} bits)")

    # Generate client keys
    priv = int.from_bytes(get_random_bytes(klen), 'big')
    cpub = pow(gen, priv, prime)
    secret = pow(spub, priv, prime)
    aes_key = hashlib.md5(secret.to_bytes(klen, 'big')).digest()

    # Prepare credentials (64 bytes username + 64 bytes password, null-terminated, random padding)
    creds = bytearray(get_random_bytes(128))
    uname_bytes = username.encode('utf-8')[:63]
    pass_bytes = password.encode('utf-8')[:63]
    creds[0:len(uname_bytes)] = uname_bytes
    creds[len(uname_bytes)] = 0
    creds[64:64+len(pass_bytes)] = pass_bytes
    creds[64+len(pass_bytes)] = 0

    # Send encrypted credentials + public key
    encrypted = AES.new(aes_key, AES.MODE_ECB).encrypt(bytes(creds))
    sock.send(encrypted + cpub.to_bytes(klen, 'big'))
    print("\n[5] Sent ARD credentials")

    # Auth result
    result = int.from_bytes(sock.recv(4), 'big')
    print(f"\n[6] Auth result: {result} {'(SUCCESS)' if result == 0 else '(FAILED)'}")

    if result != 0:
        # Try to get error message
        try:
            err_len = int.from_bytes(sock.recv(4), 'big')
            err_msg = sock.recv(err_len).decode('utf-8', errors='replace')
            print(f"    Error message: {err_msg}")
        except:
            pass
        sock.close()
        return

    # ClientInit - request shared session
    sock.send(bytes([1]))
    print("\n[7] Sent ClientInit (shared=1)")

    # ServerInit
    init = sock.recv(24)
    w = int.from_bytes(init[0:2], 'big')
    h = int.from_bytes(init[2:4], 'big')
    bpp = init[4]
    depth = init[5]
    big_endian = init[6]
    true_color = init[7]
    name_len = int.from_bytes(init[20:24], 'big')
    name = sock.recv(name_len).decode('utf-8', errors='replace')

    print(f"\n[8] ServerInit received:")
    print(f"    Screen size: {w}x{h}")
    print(f"    Bits per pixel: {bpp}")
    print(f"    Depth: {depth}")
    print(f"    Big endian: {big_endian}")
    print(f"    True color: {true_color}")
    print(f"    Desktop name: '{name}'")

    # Set pixel format (32-bit BGRA)
    sock.send(struct.pack('>BBBB BBBBHHH BBB BBB',
        0, 0, 0, 0,        # message type + padding
        32, 24, 0, 1,      # bpp, depth, big-endian, true-color
        255, 255, 255,     # max RGB values
        16, 8, 0,          # RGB shifts
        0, 0, 0))          # padding
    print("\n[9] Set pixel format: 32-bit BGRA")

    # Set encodings (raw only)
    sock.send(struct.pack('>BBH i', 2, 0, 1, 0))  # 0 = raw encoding
    print("    Set encoding: Raw (0)")

    # Request framebuffer update
    sock.send(struct.pack('>BBHHHH', 3, 0, 0, 0, w, h))
    print(f"\n[10] Requested framebuffer update for {w}x{h}")

    # Wait for response
    print("\n[11] Waiting for framebuffer data...")
    try:
        msg = sock.recv(4, socket.MSG_PEEK)
        msg_type = msg[0]
        print(f"    Message type: {msg_type}")

        if msg_type == 0:  # FramebufferUpdate
            sock.recv(1)  # consume message type
            padding = sock.recv(1)
            num_rects = int.from_bytes(sock.recv(2), 'big')
            print(f"    Number of rectangles: {num_rects}")

            total_non_black = 0
            total_pixels = 0

            for i in range(num_rects):
                rect_header = sock.recv(12)
                rx = int.from_bytes(rect_header[0:2], 'big')
                ry = int.from_bytes(rect_header[2:4], 'big')
                rw = int.from_bytes(rect_header[4:6], 'big')
                rh = int.from_bytes(rect_header[6:8], 'big')
                encoding = int.from_bytes(rect_header[8:12], 'big', signed=True)

                print(f"\n    Rectangle {i+1}:")
                print(f"      Position: ({rx}, {ry})")
                print(f"      Size: {rw}x{rh}")
                print(f"      Encoding: {encoding}")

                if encoding == 0:  # Raw
                    pixel_count = rw * rh
                    byte_count = pixel_count * 4  # 32-bit
                    print(f"      Expected bytes: {byte_count}")

                    # Read pixel data in chunks
                    pixels = b''
                    remaining = byte_count
                    while remaining > 0:
                        chunk = sock.recv(min(remaining, 65536))
                        if not chunk:
                            break
                        pixels += chunk
                        remaining -= len(chunk)

                    print(f"      Received bytes: {len(pixels)}")

                    # Analyze pixels
                    non_black = 0
                    black = 0
                    sample_colors = []

                    for j in range(0, min(len(pixels), byte_count), 4):
                        b, g, r, a = pixels[j], pixels[j+1], pixels[j+2], pixels[j+3] if j+3 < len(pixels) else 0
                        if (r, g, b) != (0, 0, 0):
                            non_black += 1
                            if len(sample_colors) < 5:
                                sample_colors.append((r, g, b))
                        else:
                            black += 1

                    total_non_black += non_black
                    total_pixels += pixel_count

                    print(f"      Black pixels: {black}")
                    print(f"      Non-black pixels: {non_black}")
                    if sample_colors:
                        print(f"      Sample non-black colors (RGB): {sample_colors}")
                else:
                    print(f"      Skipping non-raw encoding")

            print("\n" + "=" * 60)
            if total_pixels > 0:
                pct = 100 * total_non_black // total_pixels
                print(f"RESULT: {total_non_black}/{total_pixels} non-black pixels ({pct}%)")
                if pct > 0:
                    print("SUCCESS! Screen content is being captured!")
                else:
                    print("FAILED: All pixels are black")
                    print("\nPossible causes:")
                    print("  1. TCC database change requires REBOOT to take effect")
                    print("  2. Screen Sharing service needs restart")
                    print("  3. Different bundle ID needs permission")
            else:
                print("No pixel data received")

        elif msg_type == 2:  # Bell
            print("    Received Bell message")
        elif msg_type == 3:  # ServerCutText
            print("    Received ServerCutText message")
        else:
            print(f"    Unknown message type: {msg_type}")
            raw = sock.recv(100)
            print(f"    Raw data: {raw[:50].hex()}")

    except socket.timeout:
        print("    TIMEOUT waiting for response")
    except Exception as e:
        print(f"    ERROR: {e}")

    sock.close()
    print("\n[12] Connection closed")

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Test VNC connection to macOS Screen Sharing')
    parser.add_argument('-u', '--username', help='Username (or set VNC_USERNAME env var)')
    parser.add_argument('-H', '--host', default='127.0.0.1', help='VNC host (default: 127.0.0.1)')
    parser.add_argument('-p', '--port', type=int, default=5900, help='VNC port (default: 5900)')
    args = parser.parse_args()

    test_vnc(username=args.username, host=args.host, port=args.port)
