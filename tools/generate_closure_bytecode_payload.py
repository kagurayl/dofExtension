import hashlib
import json
import re
import struct
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
CNUT_MANIFEST = Path(sys.argv[1]) if len(sys.argv) > 1 else PROJECT_ROOT / "assets" / "closures" / "manifest.json"
CNUT_DIR = Path(sys.argv[2]) if len(sys.argv) > 2 else PROJECT_ROOT / "assets" / "closures"
DISPATCH = Path(sys.argv[3]) if len(sys.argv) > 3 else PROJECT_ROOT / "generated" / "current_chr_dispatch.inc"
OUT_BIN = Path(sys.argv[4]) if len(sys.argv) > 4 else PROJECT_ROOT / "generated" / "all_closures.bin"
OUT_MANIFEST = Path(sys.argv[5]) if len(sys.argv) > 5 else PROJECT_ROOT / "generated" / "all_closures_bytecode_manifest.json"

records = json.loads(CNUT_MANIFEST.read_text(encoding="utf-8"))
if not isinstance(records, list) or len(records) != 1803:
    raise ValueError(f"expected 1803 CNUT records, got {len(records) if isinstance(records, list) else 'non-list'}")

dispatch_ids = [int(value) for value in re.findall(
    r"\{0x[0-9A-Fa-f]+u,\s*(\d+)\}", DISPATCH.read_text(encoding="utf-8"))]
if len(dispatch_ids) != 1803 or len(set(dispatch_ids)) != 1803:
    raise ValueError("dispatch table must contain 1803 unique closure suffixes")

name_pattern = re.compile(r"ChangQingFunctionChar(\d+)\Z")
seen_ids = set()
seen_names = set()
payload_records = []
chunks = [b"CQBC", struct.pack("<II", 1, 1803)]

for record in records:
    closure_id = int(record["id"])
    name = record["name"]
    match = name_pattern.fullmatch(name)
    if not match or int(match.group(1)) != closure_id:
        raise ValueError(f"invalid closure name/id pair: {name}/{closure_id}")
    if closure_id in seen_ids or name in seen_names:
        raise ValueError(f"duplicate closure: {name}")
    seen_ids.add(closure_id)
    seen_names.add(name)

    stream_path = CNUT_DIR / f"{name}.cnut"
    stream = stream_path.read_bytes()
    if len(stream) < 32 or len(stream) > 1024 * 1024:
        raise ValueError(f"implausible stream size for {name}: {len(stream)}")
    if stream[:2] != b"\xFA\xFA" or b"RIQS" not in stream or b"TRAP" not in stream or not stream.endswith(b"LIAT"):
        raise ValueError(f"invalid Squirrel closure framing: {name}")

    name_bytes = name.encode("ascii")
    chunks.append(struct.pack("<III", closure_id, len(name_bytes), len(stream)))
    chunks.append(name_bytes)
    chunks.append(stream)
    payload_records.append({
        "id": closure_id,
        "name": name,
        "stream_size": len(stream),
        "stream_sha256": hashlib.sha256(stream).hexdigest(),
    })

if seen_ids != set(dispatch_ids):
    raise ValueError("CNUT closure IDs do not match current_chr_dispatch.inc")

payload = b"".join(chunks)
OUT_BIN.parent.mkdir(parents=True, exist_ok=True)
OUT_BIN.write_bytes(payload)
OUT_MANIFEST.write_text(json.dumps({
    "format": "CQBC",
    "version": 1,
    "count": 1803,
    "size": len(payload),
    "sha256": hashlib.sha256(payload).hexdigest(),
    "records": payload_records,
}, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({
    "count": 1803,
    "size": len(payload),
    "sha256": hashlib.sha256(payload).hexdigest(),
    "output": str(OUT_BIN),
}, ensure_ascii=False))
