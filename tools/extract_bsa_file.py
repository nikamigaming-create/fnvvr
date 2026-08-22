#!/usr/bin/env python3
"""List or extract exact files from a Bethesda BSA archive."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path, PureWindowsPath


def load_archive(source_root: Path, archive_path: Path):
    sys.path.insert(0, str(source_root / "local" / "pydeps"))
    from bethesda_structs.archive import BSAArchive

    return BSAArchive.parse(archive_path.read_bytes(), str(archive_path))


def entries(archive):
    file_index = 0
    for directory in archive.container.directory_blocks:
        directory_path = PureWindowsPath(directory.name[:-1])
        for record in directory.file_records:
            yield (
                str(directory_path / archive.container.file_names[file_index]),
                record,
            )
            file_index += 1


def extract_record(archive, record) -> bytes:
    file_struct = archive.uncompressed_file_struct
    if archive.container.header.archive_flags.files_compressed:
        file_struct = archive.compressed_file_struct
    if record.size > 0 and (
        archive.container.header.archive_flags.files_compressed
        != bool(record.size & archive.COMPRESSED_MASK)
    ):
        file_struct = archive.compressed_file_struct
    size = record.size & archive.SIZE_MASK
    payload = archive.content[record.offset : record.offset + size]
    if archive.container.header.archive_flags.files_prefixed:
        if not payload:
            raise ValueError("prefixed BSA record is empty")
        prefix_size = payload[0]
        if prefix_size + 1 > len(payload):
            raise ValueError("prefixed BSA record has an invalid name length")
        payload = payload[prefix_size + 1 :]
    return bytes(file_struct.parse(payload).data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", required=True)
    parser.add_argument("--match", default=".")
    parser.add_argument("--extract")
    parser.add_argument("--output")
    args = parser.parse_args()

    source_root = Path(__file__).resolve().parents[1]
    archive_path = Path(args.archive).resolve()
    archive = load_archive(source_root, archive_path)
    pattern = re.compile(args.match, re.IGNORECASE)
    requested = args.extract.replace("/", "\\").casefold() if args.extract else None

    matched = 0
    for filepath, record in entries(archive):
        normalized = filepath.replace("/", "\\")
        if requested is not None:
            if normalized.casefold() != requested:
                continue
            if not args.output:
                raise SystemExit("--output is required with --extract")
            output = Path(args.output).resolve()
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(extract_record(archive, record))
            print(f"{normalized}\t{output}\t{output.stat().st_size}")
            return 0
        if pattern.search(normalized):
            print(normalized)
            matched += 1
    return 0 if matched or requested is None else 2


if __name__ == "__main__":
    raise SystemExit(main())
