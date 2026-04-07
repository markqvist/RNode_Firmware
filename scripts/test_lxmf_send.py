#!/usr/bin/env python3
"""Send an OPPORTUNISTIC LXMF message to the R-Watch via LoRa.

Usage:
    python3 scripts/test_lxmf_send.py [-p PORT] [-m MESSAGE]

Sends a single encrypted LXMF packet to the watch's destination hash
via the T-Beam RNode. The watch should decrypt and display it on the
Messages screen.

Requires: RNS, LXMF (pip install rns lxmf)
"""

import argparse
import hashlib
import os
import struct
import time

# Watch identity (from beacon dump)
WATCH_DEST_HASH = bytes.fromhex("e76d4c209f2e0d591dc4a97864d1cc7e")
WATCH_X25519_PUB = bytes.fromhex("1c8471b6bbd403c3fd0f8e3b8cd509b428edab2360ef8d0e8a35c8e7d853546f")
WATCH_ED25519_PUB = bytes.fromhex("6068b81e12ae671b762994333d2b0c34a13637b8e6c15c38164c53ed03c175d1")

# Compute watch's identity hash = SHA256(x25519_pub + ed25519_pub)[:16]
WATCH_IDENTITY_HASH = hashlib.sha256(WATCH_X25519_PUB + WATCH_ED25519_PUB).digest()[:16]


def build_lxmf_opportunistic(message: str, dest_hash: bytes, dest_x25519_pub: bytes,
                              dest_identity_hash: bytes) -> bytes:
    """Build an OPPORTUNISTIC LXMF packet ready for LoRa transmission."""
    import RNS
    from nacl.signing import SigningKey
    from nacl.public import PrivateKey
    import nacl.bindings

    # Generate ephemeral sender identity for this message
    sender_signing_key = SigningKey.generate()
    sender_verify_key = sender_signing_key.verify_key
    sender_ed25519_pub = bytes(sender_verify_key)
    # libsodium ed25519 sk is seed(32) + pub(32) = 64 bytes
    sender_ed25519_sk_full = bytes(sender_signing_key) + sender_ed25519_pub

    # Derive X25519 keys from Ed25519
    sender_x25519_pub = nacl.bindings.crypto_sign_ed25519_pk_to_curve25519(sender_ed25519_pub)
    sender_x25519_sk = nacl.bindings.crypto_sign_ed25519_sk_to_curve25519(sender_ed25519_sk_full)

    # Compute sender's identity hash and source hash
    sender_identity_hash = hashlib.sha256(sender_x25519_pub + sender_ed25519_pub).digest()[:16]
    name_hash = hashlib.sha256(b"lxmf" + b"." + b"delivery").digest()[:10]
    sender_source_hash = hashlib.sha256(name_hash + sender_identity_hash).digest()[:16]

    # Build msgpack payload: [timestamp, title, content, {}]
    import msgpack
    timestamp = int(time.time())
    payload = msgpack.packb([timestamp, b"", message.encode("utf-8"), {}])

    # Build LXMF plaintext: source_hash(16) + signature(64) + payload
    # Signature covers: dest_hash + source_hash + payload + SHA256(same)
    hashed_part = dest_hash + sender_source_hash + payload
    message_hash = hashlib.sha256(hashed_part).digest()
    signed_part = hashed_part + message_hash
    signature = sender_signing_key.sign(signed_part).signature

    lxmf_plaintext = sender_source_hash + signature + payload
    print(f"LXMF plaintext: {len(lxmf_plaintext)} bytes")
    print(f"  source_hash: {sender_source_hash.hex()}")
    print(f"  content: \"{message}\"")

    # Encrypt with Token format: ephemeral_pub(32) + IV(16) + ciphertext + HMAC(32)
    # ECDH with destination's X25519 public key
    shared_secret = nacl.bindings.crypto_scalarmult(sender_x25519_sk, dest_x25519_pub)

    # HKDF: derive signing_key(32) + encryption_key(32)
    derived = RNS.Cryptography.hkdf(
        length=64,
        derive_from=shared_secret,
        salt=dest_identity_hash,
        context=None
    )
    signing_key = derived[:32]
    encryption_key = derived[32:]

    # Generate ephemeral X25519 keypair for the Token
    eph_private = PrivateKey.generate()
    eph_public = bytes(eph_private.public_key)
    eph_shared = nacl.bindings.crypto_scalarmult(bytes(eph_private), dest_x25519_pub)

    # Re-derive keys using ephemeral (this is what the receiver will do)
    derived2 = RNS.Cryptography.hkdf(
        length=64,
        derive_from=eph_shared,
        salt=dest_identity_hash,
        context=None
    )
    token_signing_key = derived2[:32]
    token_encryption_key = derived2[32:]

    # AES-256-CBC encrypt with PKCS7 padding
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.primitives import padding as sym_padding

    iv = os.urandom(16)
    padder = sym_padding.PKCS7(128).padder()
    padded = padder.update(lxmf_plaintext) + padder.finalize()

    cipher = Cipher(algorithms.AES(token_encryption_key), modes.CBC(iv))
    encryptor = cipher.encryptor()
    ciphertext = encryptor.update(padded) + encryptor.finalize()

    # HMAC-SHA256(signing_key, IV || ciphertext)
    import hmac as hmac_mod
    h = hmac_mod.new(token_signing_key, iv + ciphertext, hashlib.sha256)
    hmac_tag = h.digest()

    # Token: ephemeral_pub(32) + IV(16) + ciphertext + HMAC(32)
    encrypted = eph_public + iv + ciphertext + hmac_tag
    print(f"Encrypted payload: {len(encrypted)} bytes")

    # RNS packet: flags(1) + hops(1) + dest_hash(16) + context(1) + encrypted
    rns_flags = 0x00  # HEADER_1, BROADCAST, SINGLE, DATA
    rns_hops = 0x00
    rns_context = 0x00

    packet = bytes([rns_flags, rns_hops]) + dest_hash + bytes([rns_context]) + encrypted
    print(f"Total RNS packet: {len(packet)} bytes")

    if len(packet) > 255:
        print(f"WARNING: packet too large for single LoRa frame ({len(packet)} > 255)")

    return packet


def send_via_kiss(port: str, packet: bytes):
    """Send a raw packet via KISS to an RNode."""
    import serial

    FEND = 0xC0
    FESC = 0xDB
    TFEND = 0xDC
    TFESC = 0xDD
    CMD_DATA = 0x00

    # KISS escape
    escaped = bytearray()
    for b in packet:
        if b == FEND:
            escaped += bytes([FESC, TFEND])
        elif b == FESC:
            escaped += bytes([FESC, TFESC])
        else:
            escaped.append(b)

    frame = bytes([FEND, CMD_DATA]) + bytes(escaped) + bytes([FEND])

    s = serial.Serial(port, 115200, timeout=5)
    time.sleep(0.5)
    s.write(frame)
    s.flush()
    print(f"Sent {len(frame)} bytes KISS frame to {port}")
    time.sleep(2)
    s.close()


def main():
    parser = argparse.ArgumentParser(description="Send LXMF message to R-Watch")
    parser.add_argument("-p", "--port", default="/dev/ttyACM0",
                        help="T-Beam RNode serial port")
    parser.add_argument("-m", "--message", default="Hello from test!",
                        help="Message content")
    args = parser.parse_args()

    print(f"Sending to watch dest: {WATCH_DEST_HASH.hex()}")
    print(f"Watch identity hash:   {WATCH_IDENTITY_HASH.hex()}")
    print()

    packet = build_lxmf_opportunistic(
        args.message,
        WATCH_DEST_HASH,
        WATCH_X25519_PUB,
        WATCH_IDENTITY_HASH
    )

    print()
    send_via_kiss(args.port, packet)
    print("Done! Check watch Messages screen.")


if __name__ == "__main__":
    main()
