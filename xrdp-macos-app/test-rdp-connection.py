#!/usr/bin/env python3
"""
Simple RDP connection test script
Tests if xrdp server accepts connections and performs TLS handshake
"""
import socket
import ssl
import sys
import time

def test_rdp_connection(host='127.0.0.1', port=3389):
    """Test RDP connection to xrdp server"""
    print(f"Testing RDP connection to {host}:{port}...")

    try:
        # Create socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)

        print("Connecting...")
        sock.connect((host, port))
        print("✓ TCP connection established")

        # Send RDP connection request (X.224 Connection Request)
        # This is a minimal RDP connection request PDU
        x224_connection_request = bytes([
            # TPKT Header
            0x03,  # Version
            0x00,  # Reserved
            0x00, 0x13,  # Length (19 bytes)
            # X.224 Connection Request
            0x0e,  # Length
            0xe0,  # Connection Request
            0x00, 0x00,  # Destination reference
            0x00, 0x00,  # Source reference
            0x00,  # Class option
            # RDP Negotiation Request (RDP_NEG_REQ)
            0x01,  # Type
            0x00,  # Flags
            0x08, 0x00,  # Length (8 bytes)
            0x03, 0x00, 0x00, 0x00  # Requested protocols (SSL | HYBRID | HYBRID_EX)
        ])

        print("Sending X.224 Connection Request...")
        sock.sendall(x224_connection_request)

        print("Waiting for response...")
        response = sock.recv(1024)

        if len(response) > 0:
            print(f"✓ Received response ({len(response)} bytes)")
            print(f"  Response: {response.hex()}")

            # Check if TLS negotiation was successful
            # Look for RDP_NEG_RSP (type 0x02) in response
            if b'\x02' in response[11:15]:  # Type field location
                print("✓ Server accepted TLS negotiation")

                # Attempt TLS handshake
                print("Attempting TLS handshake...")
                try:
                    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
                    context.check_hostname = False
                    context.verify_mode = ssl.CERT_NONE
                    # Force TLS 1.3 only
                    context.minimum_version = ssl.TLSVersion.TLSv1_3
                    context.maximum_version = ssl.TLSVersion.TLSv1_3
                    # Set TLS 1.3 ciphersuites (not set_ciphers for TLS 1.3)
                    context.set_ciphers('TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256')

                    tls_sock = context.wrap_socket(sock, server_hostname=host)
                    print("✓ TLS handshake successful!")
                    print(f"  TLS version: {tls_sock.version()}")
                    print(f"  Cipher: {tls_sock.cipher()}")

                    tls_sock.close()
                    return True
                except Exception as e:
                    print(f"✗ TLS handshake failed: {e}")
                    return False
            else:
                print("✗ Server did not accept TLS negotiation")
                return False
        else:
            print("✗ No response from server")
            return False

    except socket.timeout:
        print("✗ Connection timed out")
        return False
    except ConnectionRefusedError:
        print("✗ Connection refused - is xrdp running?")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        try:
            sock.close()
        except:
            pass

if __name__ == '__main__':
    host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 3389

    success = test_rdp_connection(host, port)

    if success:
        print("\n✓ RDP connection test PASSED")
        sys.exit(0)
    else:
        print("\n✗ RDP connection test FAILED")
        sys.exit(1)
