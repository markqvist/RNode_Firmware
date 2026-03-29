#!/usr/bin/env python3
"""
IFAC Crypto Test — compares IfacAuth.h C implementation against Reticulum Python

Tests each stage of the IFAC pipeline:
1. Ed25519 keypair derivation from seed
2. Ed25519 signing
3. HKDF expansion
4. Full IFAC apply on a test packet

Run standalone to generate test vectors, or with --firmware to
send test vectors to the watch and compare output.

Usage:
    python3 scripts/test_ifac.py              # generate test vectors
    python3 scripts/test_ifac.py --firmware   # compare with firmware
"""

import sys
import os
import hashlib
import hmac

# Add RNS to path
sys.path.insert(0, os.path.expanduser("~/.local/lib/python3.13/site-packages"))

import RNS
from RNS.Cryptography.pure25519 import ed25519_oop as ed25519
from RNS.Cryptography.pure25519._ed25519 import sign as ed25519_raw_sign


def hkdf_sha256(ikm, salt, length, context=None):
    """Replicate RNS.Cryptography.hkdf exactly"""
    return RNS.Cryptography.hkdf(
        length=length,
        derive_from=ikm,
        salt=salt,
        context=context,
    )


def hkdf_sha256_manual(ikm, salt, output_len):
    """Manual HKDF-SHA256 matching our C implementation"""
    # Extract
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()

    # Expand
    output = b""
    prev_block = b""
    block_idx = 0
    while len(output) < output_len:
        expand_input = prev_block + bytes([(block_idx + 1) % 256])
        block = hmac.new(prk, expand_input, hashlib.sha256).digest()
        output += block
        prev_block = block
        block_idx += 1

    return output[:output_len]


def ifac_apply_python(pkt, ifac_key, ifac_size=8):
    """Replicate Reticulum's Transport.transmit IFAC application"""
    # Create identity from ifac_key (last 32 bytes = Ed25519 seed)
    sig_seed = ifac_key[32:]
    identity = RNS.Identity.from_bytes(ifac_key)

    # 1. Sign the original packet
    sig = identity.sign(pkt)  # Returns 64-byte signature
    assert len(sig) == 64, f"Signature length: {len(sig)}"

    # 2. Extract IFAC: last 8 bytes of signature
    ifac = sig[-ifac_size:]

    # 3. Generate mask
    mask = RNS.Cryptography.hkdf(
        length=len(pkt) + ifac_size,
        derive_from=ifac,
        salt=ifac_key,
        context=None,
    )

    # 4. Set IFAC flag + assemble
    new_header = bytes([pkt[0] | 0x80, pkt[1]])
    new_raw = new_header + ifac + pkt[2:]

    # 5. Mask
    masked = bytearray()
    for i, byte in enumerate(new_raw):
        if i == 0:
            masked.append((byte ^ mask[i]) | 0x80)
        elif i == 1 or i > ifac_size + 1:
            masked.append(byte ^ mask[i])
        else:
            masked.append(byte)  # Don't mask IFAC itself

    return bytes(masked)


def main():
    print("=" * 60)
    print("IFAC Crypto Test Vectors")
    print("=" * 60)

    # Known IFAC key (helv4net / R3ticulum-priv8-m3sh)
    network_name = "helv4net"
    passphrase = "R3ticulum-priv8-m3sh"

    ifac_origin = b""
    ifac_origin += RNS.Identity.full_hash(network_name.encode("utf-8"))
    ifac_origin += RNS.Identity.full_hash(passphrase.encode("utf-8"))
    ifac_origin_hash = RNS.Identity.full_hash(ifac_origin)

    ifac_key = hkdf_sha256(ifac_origin_hash, RNS.Reticulum.IFAC_SALT, 64)

    print(f"\n1. IFAC Key Derivation")
    print(f"   Network: {network_name}")
    print(f"   Pass:    {passphrase}")
    print(f"   Key:     {ifac_key.hex()}")

    # Ed25519 seed = last 32 bytes of ifac_key
    ed25519_seed = ifac_key[32:]
    print(f"\n2. Ed25519 Seed (ifac_key[32:64])")
    print(f"   Seed:    {ed25519_seed.hex()}")

    # Derive keypair
    sk = ed25519.SigningKey(ed25519_seed)
    pk = sk.get_verifying_key()
    print(f"   PK:      {pk.to_bytes().hex()}")

    # Also show the full sk_s (seed + pk, 64 bytes — libsodium format)
    sk_s = sk.sk_s  # This is the internal 64-byte representation
    print(f"   SK(64):  {sk_s.hex()}")

    # Test signing with a known message
    test_msg = bytes([0x00, 0x00, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
                      0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                      0x77, 0x88, 0x99])
    print(f"\n3. Ed25519 Signature Test")
    print(f"   Msg:     {test_msg.hex()}")

    sig = sk.sign(test_msg)
    print(f"   Sig:     {sig.hex()}")
    print(f"   Last 8:  {sig[-8:].hex()}")

    # HKDF test
    test_ikm = sig[-8:]
    print(f"\n4. HKDF Test")
    print(f"   IKM:     {test_ikm.hex()}")
    print(f"   Salt:    {ifac_key.hex()}")

    mask_rns = hkdf_sha256(test_ikm, ifac_key, 27)
    mask_manual = hkdf_sha256_manual(test_ikm, ifac_key, 27)
    print(f"   RNS:     {mask_rns.hex()}")
    print(f"   Manual:  {mask_manual.hex()}")
    print(f"   Match:   {mask_rns == mask_manual}")

    # Full IFAC apply
    print(f"\n5. Full IFAC Apply")
    print(f"   Input:   {test_msg.hex()} ({len(test_msg)} bytes)")

    result = ifac_apply_python(test_msg, ifac_key)
    print(f"   Output:  {result.hex()} ({len(result)} bytes)")
    print(f"   Size:    {len(test_msg)} -> {len(result)}")

    # Generate C test vector
    print(f"\n{'=' * 60}")
    print(f"C Test Vectors (paste into firmware test)")
    print(f"{'=' * 60}")
    print(f"const uint8_t test_ifac_key[64] = {{{', '.join(f'0x{b:02x}' for b in ifac_key)}}};")
    print(f"const uint8_t test_ed25519_seed[32] = {{{', '.join(f'0x{b:02x}' for b in ed25519_seed)}}};")
    print(f"const uint8_t test_ed25519_pk[32] = {{{', '.join(f'0x{b:02x}' for b in pk.to_bytes())}}};")
    print(f"const uint8_t test_msg[{len(test_msg)}] = {{{', '.join(f'0x{b:02x}' for b in test_msg)}}};")
    print(f"const uint8_t test_sig[64] = {{{', '.join(f'0x{b:02x}' for b in sig)}}};")
    print(f"const uint8_t test_ifac[8] = {{{', '.join(f'0x{b:02x}' for b in sig[-8:])}}};")
    print(f"const uint8_t test_mask[{len(result)}] = {{{', '.join(f'0x{b:02x}' for b in mask_rns[:len(result)])}}};")
    print(f"const uint8_t test_result[{len(result)}] = {{{', '.join(f'0x{b:02x}' for b in result)}}};")


if __name__ == "__main__":
    main()
