#!/usr/bin/env python3
"""Test vectors for the alcor-chain EM and ED keys (test_em_ed.cpp and unittests/alcor_keys_tests.cpp).

Independent of fc: secp256k1 is plain Python integer math with an RFC 6979 nonce (what libsecp256k1 uses, so fc must
produce the same bytes), keccak-256 and Ed25519 come from pycryptodome (`pip install pycryptodome`).

Self-checks before printing anything:
  - EIP-191 + RFC 6979 against the web3.js documentation example (accounts.sign('Some data', key))
  - Ed25519 against RFC 8032 section 7.1 test vector 1
"""
import hashlib
import hmac

from Crypto.Hash import keccak
from Crypto.PublicKey import ECC
from Crypto.Signature import eddsa

# ---- secp256k1

P = 2**256 - 2**32 - 977
N = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
G = (0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798,
     0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8)


def add(a, b):
    if a is None:
        return b
    if b is None:
        return a
    if a[0] == b[0] and (a[1] + b[1]) % P == 0:
        return None
    if a == b:
        m = 3 * a[0] * a[0] * pow(2 * a[1], -1, P)
    else:
        m = (b[1] - a[1]) * pow(b[0] - a[0], -1, P)
    x = (m * m - a[0] - b[0]) % P
    return x, (m * (a[0] - x) - a[1]) % P


def mul(k, point=G):
    result = None
    while k:
        if k & 1:
            result = add(result, point)
        point = add(point, point)
        k >>= 1
    return result


def compressed(point):
    return bytes([2 + (point[1] & 1)]) + point[0].to_bytes(32, 'big')


def uncompressed(point):
    return b'\x04' + point[0].to_bytes(32, 'big') + point[1].to_bytes(32, 'big')


def rfc6979_nonce(secret, h):
    x = secret.to_bytes(32, 'big')
    h1 = (int.from_bytes(h, 'big') % N).to_bytes(32, 'big')
    v, k = b'\x01' * 32, b'\x00' * 32
    k = hmac.new(k, v + b'\x00' + x + h1, hashlib.sha256).digest()
    v = hmac.new(k, v, hashlib.sha256).digest()
    k = hmac.new(k, v + b'\x01' + x + h1, hashlib.sha256).digest()
    v = hmac.new(k, v, hashlib.sha256).digest()
    while True:
        v = hmac.new(k, v, hashlib.sha256).digest()
        nonce = int.from_bytes(v, 'big')
        if 0 < nonce < N:
            return nonce
        k = hmac.new(k, v + b'\x00', hashlib.sha256).digest()
        v = hmac.new(k, v, hashlib.sha256).digest()


def ecdsa_sign(secret, h):
    """r ‖ s ‖ v, low-s, v = 27 + recovery id, as libsecp256k1 + Ethereum."""
    nonce = rfc6979_nonce(secret, h)
    point = mul(nonce)
    r = point[0] % N
    s = pow(nonce, -1, N) * (int.from_bytes(h, 'big') + r * secret) % N
    recid = (point[1] & 1) | (2 if point[0] >= N else 0)
    if s > N // 2:
        s, recid = N - s, recid ^ 1
    return r.to_bytes(32, 'big') + s.to_bytes(32, 'big') + bytes([27 + recid])


def keccak256(data):
    return keccak.new(digest_bits=256, data=data).digest()


def personal_message(text):
    return b'\x19Ethereum Signed Message:\n' + str(len(text)).encode() + text


# ---- alcor-chain signing text

PREFIX = b'Alcor Chain transaction\nSign only in an app you trust\n'


def signing_text(digest):
    return PREFIX + digest.hex().encode()


# ---- Ed25519

def ed_sign(seed, message):
    key = ECC.construct(curve='ed25519', seed=seed)
    return key.public_key().export_key(format='raw'), eddsa.new(key, 'rfc8032').sign(message)


# ---- base58 (no checksum)

B58 = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'


def base58(data):
    n = int.from_bytes(data, 'big')
    out = ''
    while n:
        n, r = divmod(n, 58)
        out = B58[r] + out
    return '1' * (len(data) - len(data.lstrip(b'\0'))) + out


def self_check():
    key = 0x4c0883a69102937d6231471b5dbb6204fe5129617082792ae468d01a3f362318
    h = keccak256(personal_message(b'Some data'))
    assert h.hex() == '1da44b586eb0729ff70a73c326926f6ed5a25f5b056e7f47fbc6e58d86871655'
    assert ecdsa_sign(key, h).hex() == ('b91467e570a6466aa9e9876cbcd013baba02900b8979d43fe208a4a4f339f5fd'
                                        '6007e74cd82e037b800186422fc2da167c747ef045e5d18a5f5d4300f8e1a0291c')
    pub, sig = ed_sign(bytes.fromhex('9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60'), b'')
    assert pub.hex() == 'd75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a'
    assert sig.hex() == ('e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555'
                         'fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b')


def main():
    self_check()
    digest = hashlib.sha256(b'alcor-chain EM/ED test digest').digest()
    text = signing_text(digest)
    print('digest       ', digest.hex())
    print('signing text ', repr(text.decode()))

    # EM: secret = sha256("alcor-chain EM test key")
    secret = int.from_bytes(hashlib.sha256(b'alcor-chain EM test key').digest(), 'big')
    pub = mul(secret)
    h = keccak256(personal_message(text))
    sig = ecdsa_sign(secret, h)
    print('EM secret    ', 'PVT_EM_0x' + secret.to_bytes(32, 'big').hex())
    print('EM public    ', 'PUB_EM_' + uncompressed(pub).hex())
    print('EM compressed', compressed(pub).hex())
    print('EM msg hash  ', h.hex())
    print('EM signature ', 'SIG_EM_0x' + sig.hex())

    # ED: seed = sha256("alcor-chain ED test key")
    seed = hashlib.sha256(b'alcor-chain ED test key').digest()
    ed_pub, ed_sig = ed_sign(seed, text)
    print('ED secret    ', 'PVT_ED_' + base58(seed + ed_pub))
    print('ED public    ', 'PUB_ED_' + base58(ed_pub))
    print('ED public hex', ed_pub.hex())
    print('ED signature ', 'SIG_ED_' + base58(ed_pub + ed_sig))
    print('ED sig hex   ', ed_sig.hex())


if __name__ == '__main__':
    main()
