#!/usr/bin/env python3
"""Minimal APK builder: deterministic ZIP writer, 16 KB alignment, v1+v2 signing.

No Android build tools required beyond aapt2 for resource linking. This module
implements the parts of zipalign / apksigner / jarsigner that we need:

  * entries written with correct 16 KB alignment for uncompressed native libs
    (required by Android 15's 16 KB page size devices)
  * JAR (v1) signing: MANIFEST.MF + CERT.SF + CERT.RSA (PKCS#7, detached)
  * APK Signature Scheme v2 block (APK Signing Block, 0x7109871a)
"""
import hashlib
import os
import struct
import zlib

# --------------------------------------------------------------------------- ZIP
class ZipBuilder:
    def __init__(self):
        self.entries = []   # (name, data, method, align)
        self.cd = []

    def add(self, name, data, method="deflate", align=4):
        self.entries.append((name, data, method, align))

    def _pad_extra(self, extra_len, data_offset_probe, align):
        """Extra field bytes that push the data offset to `align`."""
        if align <= 1:
            return b""
        # Local header is 30 bytes + name (already counted in probe), so pad
        pad = (-data_offset_probe) % align
        if pad < 4:
            pad += align * ((4 - pad + align - 1) // align) if pad < 4 else 0
        if pad < 4:
            pad = 4
        # extra field: header id 0xD935 (zipalign style) + length + filler
        body_len = pad - 4
        return struct.pack("<HH", 0xD935, body_len) + b"\0" * body_len

    def build(self):
        out = bytearray()
        cd = bytearray()
        for name, data, method, align in self.entries:
            name_b = name.encode("utf-8")
            if method == "deflate":
                comp = zlib.compressobj(9, zlib.DEFLATED, -15)
                payload = comp.compress(data) + comp.flush()
                method_id = 8
            else:
                payload = data
                method_id = 0
            crc = zlib.crc32(data) & 0xFFFFFFFF
            extra = self._pad_extra(len(out) + 30 + len(name_b), len(out) + 30 + len(name_b), align)
            hdr = struct.pack(
                "<IHHHHHIIIHH",
                0x04034B50, 20, 0, method_id, 0, 0x21,  # time/date fixed for reproducibility
                crc, len(payload), len(data), len(name_b), len(extra),
            )
            offset = len(out)
            out += hdr + name_b + extra + payload
            cd += struct.pack(
                "<IHHHHHHIIIHHHHHII",
                0x02014B50, 20, 20, 0, method_id, 0, 0x21,
                crc, len(payload), len(data), len(name_b), len(extra), 0,
                0, 0, 0, offset,
            ) + name_b + extra
        cd_offset = len(out)
        out += cd
        eocd = struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, len(self.entries), len(self.entries),
                           len(cd), cd_offset, 0)
        out += eocd
        return bytes(out), cd_offset, len(cd)


def read_zip_structure(apk):
    """Parse central directory, returning entries and offsets."""
    eocd_off = apk.rfind(b"PK\x05\x06")
    if eocd_off < 0:
        raise ValueError("no EOCD")
    (_, _, _, total, _, cd_size, cd_offset, _) = struct.unpack_from("<IHHHHIIH", apk, eocd_off)
    entries = []
    p = cd_offset
    for _ in range(total):
        (sig, _, _, _, method, _, _, crc, csize, usize, nlen, elen, clen, _, _, _, lho) = \
            struct.unpack_from("<IHHHHHHIIIHHHHHII", apk, p)
        assert sig == 0x02014B50, "bad central directory signature"
        name = apk[p + 46:p + 46 + nlen].decode("utf-8")
        entries.append(dict(name=name, method=method, crc=crc, csize=csize, usize=usize,
                            local_offset=lho, cd_record=apk[p:p + 46 + nlen + elen + clen]))
        p += 46 + nlen + elen + clen
    return entries, cd_offset, cd_size, eocd_off


def local_entry_bytes(apk, entry):
    """Return the full local header + data bytes for one entry."""
    off = entry["local_offset"]
    (sig, _, _, method, _, _, _, crc, csize, usize, nlen, elen) = struct.unpack_from("<IHHHHHIIIHH", apk, off)
    assert sig == 0x04034B50
    start = off
    end = off + 30 + nlen + elen + csize
    return apk[start:end]


# ------------------------------------------------------- v1 (JAR) signature
def v1_sign(entries_data, key, cert_der, alias="cyberdrift"):
    """Build MANIFEST.MF / CERT.SF / CERT.RSA for the given entries."""
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.serialization import pkcs7

    cert_obj = x509.load_der_x509_certificate(cert_der)

    manifest = b"Manifest-Version: 1.0\r\nCreated-By: CyberDrift Build\r\n\r\n"
    names = sorted(entries_data.keys())
    for name in names:
        digest = hashlib.sha256(entries_data[name]).digest()
        import base64
        b64 = base64.b64encode(digest).decode()
        manifest += f"Name: {name}\r\nSHA-256-Digest: {b64}\r\n\r\n".encode()

    import base64
    mdigest = base64.b64encode(hashlib.sha256(manifest).digest()).decode()
    sf = "Signature-Version: 1.0\r\nCreated-By: CyberDrift Build\r\nSHA-256-Digest-Manifest: {}\r\n\r\n".format(mdigest)
    for name in names:
        # per-entry section digest of the manifest's entry section
        section = b""
        for line in manifest.split(b"\r\n\r\n"):
            if line.startswith(b"Name: " + name.encode()):
                section = line + b"\r\n"
                break
        if section:
            d = base64.b64encode(hashlib.sha256(section).digest()).decode()
            sf += "Name: {}\r\nSHA-256-Digest: {}\r\n\r\n".format(name, d)
    sf_b = sf.encode()

    builder = pkcs7.PKCS7SignatureBuilder().set_data(sf_b).add_signer(cert_obj, key, hashes.SHA256())
    sig = builder.sign(serialization.Encoding.DER, [pkcs7.PKCS7Options.DetachedSignature])
    return manifest, sf_b, sig


# ------------------------------------------------------- v2 signature block
def _lp(data):
    return struct.pack("<I", len(data)) + data


def _digest_chunked(data):
    """SHA-256 over the chunked digest structure used by scheme v2."""
    chunk = 1024 * 1024
    h = hashlib.sha256()
    for i in range(0, len(data), chunk):
        part = data[i:i + chunk]
        d = hashlib.sha256(part).digest()
        h.update(b"\xa5" + struct.pack("<I", len(part)) + d)
    return h.digest()


def v2_sign(apk_bytes, key, cert_der):
    """Insert an APK Signing Block (scheme v2) into a finished APK."""
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import padding
    from cryptography.hazmat.primitives import serialization

    entries, cd_offset, cd_size, eocd_off = read_zip_structure(apk_bytes)
    contents = apk_bytes[:cd_offset]              # ZIP entries region
    central = apk_bytes[cd_offset:cd_offset + cd_size]
    eocd = bytearray(apk_bytes[eocd_off:])
    # Replace the central directory offset in the EOCD with our signing block offset.
    struct.pack_into("<I", eocd, 16, cd_offset)

    digest_entries = _digest_chunked(contents)
    digest_cd = _digest_chunked(bytes(central))
    digest_eocd = _digest_chunked(bytes(eocd))
    final = hashlib.sha256(b"\x5a" + struct.pack("<I", 3) +
                           digest_entries + digest_cd + digest_eocd).digest()

    ALG_RSA_PKCS1_SHA256 = 0x0103
    digests = _lp(struct.pack("<I", ALG_RSA_PKCS1_SHA256) + _lp(final))
    certs = _lp(cert_der)
    attrs = _lp(b"")
    signed_data = _lp(digests + certs + attrs)

    signature = key.sign(signed_data, padding.PKCS1v15(), hashes.SHA256())
    signatures = _lp(struct.pack("<I", ALG_RSA_PKCS1_SHA256) + _lp(signature))
    pubkey = key.public_key().public_bytes(serialization.Encoding.DER,
                                           serialization.PublicFormat.SubjectPublicKeyInfo)
    signer = _lp(signed_data) + _lp(signatures) + _lp(pubkey)
    signers = _lp(signer)

    # value block: length-prefixed sequence of signers
    value = signers
    pair = struct.pack("<I", 0x7109871A) + value
    pair_block = struct.pack("<Q", len(pair) + 8) + pair
    size = len(pair_block) + 8 + 16
    block = (struct.pack("<Q", size) + pair_block + struct.pack("<Q", size) +
             b"APK Sig Block 42")

    new_cd_offset = cd_offset + len(block)
    out = bytearray()
    out += contents
    out += block
    out += central
    new_eocd = bytearray(apk_bytes[eocd_off:])
    struct.pack_into("<I", new_eocd, 16, new_cd_offset)
    out += new_eocd
    return bytes(out)


# ------------------------------------------------------------------ helpers
def make_key_and_cert(common_name="Cyber Drift", org="Arena Interactive"):
    from datetime import datetime, timedelta, timezone
    from cryptography import x509
    from cryptography.x509.oid import NameOID
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa

    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    subject = issuer = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, org),
        x509.NameAttribute(NameOID.COUNTRY_NAME, "IN"),
    ])
    now = datetime.now(timezone.utc)
    cert = (x509.CertificateBuilder()
            .subject_name(subject).issuer_name(issuer)
            .public_key(key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now - timedelta(days=1))
            .not_valid_after(now + timedelta(days=3650))
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .sign(key, hashes.SHA256()))
    return key, cert.public_bytes(serialization.Encoding.DER)
