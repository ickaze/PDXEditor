#!/usr/bin/env python3
"""Validate an MSVC Release PE and create source/runtime release ZIPs."""
from pathlib import Path
import hashlib
import os
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "1.2.0"
EXE = ROOT / "x64/Release/PDXEditor.exe"

def inspect_pe(data):
    def u16(at): return struct.unpack_from("<H", data, at)[0]
    def u32(at): return struct.unpack_from("<I", data, at)[0]
    assert data[:2] == b"MZ", "Not a PE file"
    pe = u32(0x3c)
    assert data[pe:pe+4] == b"PE\0\0" and u16(pe+4) == 0x8664, "Not AMD64"
    optional = pe + 24
    assert u16(optional) == 0x20b, "Not PE32+"
    directory = optional + 112
    assert u32(directory+6*8) == 0 and u32(directory+6*8+4) == 0, "Debug directory embedded"
    sections = []
    table = optional + u16(pe+20)
    for i in range(u16(pe+6)):
        at = table + 40*i
        sections.append((u32(at+12), max(u32(at+8), u32(at+16)), u32(at+20)))
    def offset(rva):
        for va, size, raw in sections:
            if va <= rva < va+size:
                return raw+rva-va
        raise AssertionError(f"Unmapped RVA {rva:x}")
    imports = []
    rva = u32(directory+8)
    assert rva, "Import directory absent"
    at = offset(rva)
    while any(data[at:at+20]):
        name = offset(u32(at+12))
        end = data.index(b"\0", name)
        imports.append(data[name:end].decode("ascii").lower())
        at += 20
    forbidden = ("msvcp", "vcruntime", "msvcr", "ucrtbase", "api-ms-win-crt")
    assert not any(s.startswith(forbidden) for s in imports), imports
    return imports

def make_zip(path, files):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(f"PDXEditor_v{VERSION}/{name}", data)
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        forbidden = (".pdb", ".obj", ".ilk", ".ini", ".pdxedit")
        assert not any(n.lower().endswith(forbidden) for n in archive.namelist())

def main():
    imports = inspect_pe(EXE.read_bytes())
    commit = os.environ.get("GITHUB_SHA", "local build")
    report = (f"PDXEditor v{VERSION}\nCommit: {commit}\n"
              "Windows x64 MSVC Release build: passed\n"
              "PE AMD64: passed\nExternal VC++ runtime imports: none\n"
              "PE debug directory: absent\nZIP contents and CRC: verified\n"
              "Portable core and NDSIF mock checks: passed in prerequisite CI job\n"
              "Windows GUI, MIDI, audible output and NanoDrive8 hardware: not tested\n"
              "Imports: " + ", ".join(imports) + "\n").encode()
    docs = ["LICENSE", "README.md", "CHANGELOG.md", "RELEASE_NOTES.md",
            "THIRD_PARTY_NOTICES.md", "VALIDATION.md"]
    runtime = {name: (ROOT/name).read_bytes() for name in docs}
    runtime["PDXEditor.exe"] = EXE.read_bytes()
    for lang in (ROOT/"languages").glob("*.lang"):
        runtime["languages/"+lang.name] = lang.read_bytes()
    runtime["BUILD_VALIDATION.txt"] = report
    runtime["SHA256SUMS.txt"] = "".join(
        hashlib.sha256(data).hexdigest()+"  "+name+"\n"
        for name, data in sorted(runtime.items())).encode()
    source = {}
    for name in ["src", "tests", "languages", ".github"]:
        for path in (ROOT/name).rglob("*"):
            if path.is_file() and "__pycache__" not in path.parts:
                source[path.relative_to(ROOT).as_posix()] = path.read_bytes()
    for name in docs+["CMakeLists.txt", "PDXEditor.sln", "PDXEditor.vcxproj",
                      "build_vs2022.bat", ".clang-format", ".gitignore"]:
        source[name] = (ROOT/name).read_bytes()
    source["SHA256SUMS.txt"] = "".join(
        hashlib.sha256(data).hexdigest()+"  "+name+"\n"
        for name, data in sorted(source.items())).encode()
    dist = ROOT/"dist"
    dist.mkdir(exist_ok=True)
    make_zip(dist/f"PDXEditor_v{VERSION}_Windows_x64.zip", runtime)
    make_zip(dist/f"PDXEditor_v{VERSION}_source.zip", source)
    (dist/"SHA256SUMS.txt").write_text("".join(
        hashlib.sha256(path.read_bytes()).hexdigest()+"  "+path.name+"\n"
        for path in sorted(dist.glob("*.zip"))), encoding="ascii")
    print(report.decode())

if __name__ == "__main__":
    main()
