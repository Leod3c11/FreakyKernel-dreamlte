#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Repack a Samsung legacy Android boot image while preserving its base."""

import argparse
import hashlib
import os
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


BOOT_MAGIC = b"ANDROID!"
SAMSUNG_MARKER = b"SEANDROIDENFORCE"
HEADER = struct.Struct("<8s10I16s512s32s")
ID_OFFSET = 576
ID_SIZE = 32
KERNEL_SIZE_OFFSET = 8
DT_SIZE_OFFSET = 40
ARM64_IMAGE_MAGIC_OFFSET = 56
ARM64_IMAGE_MAGIC = b"ARM\x64"


class BootImageError(Exception):
	"""The input cannot safely be handled as a Samsung legacy boot image."""


def align(value: int, alignment: int) -> int:
	return (value + alignment - 1) // alignment * alignment


def human_size(value: int) -> str:
	return f"{value / (1024 * 1024):.2f} MiB"


def kernel_format(payload: bytes) -> str:
	if payload.startswith(b"\x1f\x8b"):
		return "gzip"
	magic_end = ARM64_IMAGE_MAGIC_OFFSET + len(ARM64_IMAGE_MAGIC)
	if len(payload) >= magic_end and payload[ARM64_IMAGE_MAGIC_OFFSET:magic_end] == ARM64_IMAGE_MAGIC:
		return "raw"
	raise BootImageError("kernel payload is neither an arm64 Image nor Image.gz")


@dataclass
class LegacyBootImage:
	path: Path
	raw: bytes
	header_page: bytes
	kernel: bytes
	ramdisk: bytes
	second: bytes
	dtb: bytes
	page_size: int
	kernel_address: int
	ramdisk_address: int
	second_address: int
	tags_address: int
	marker: bytes
	partition_padding: int
	stored_id: bytes

	@classmethod
	def load(cls, path: Path) -> "LegacyBootImage":
		raw = path.read_bytes()
		if len(raw) < HEADER.size:
			raise BootImageError("image is smaller than the legacy boot header")

		unpacked = HEADER.unpack_from(raw)
		magic = unpacked[0]
		(
			kernel_size,
			kernel_address,
			ramdisk_size,
			ramdisk_address,
			second_size,
			second_address,
			tags_address,
			page_size,
			dt_size,
			_unused,
		) = unpacked[1:11]

		if magic != BOOT_MAGIC:
			raise BootImageError("missing ANDROID! boot magic")
		if page_size < HEADER.size or page_size & (page_size - 1):
			raise BootImageError(f"invalid legacy page size: {page_size}")
		if page_size > len(raw):
			raise BootImageError("boot header page extends beyond the image")
		if kernel_size == 0 or ramdisk_size == 0:
			raise BootImageError("base image has an empty kernel or ramdisk")

		position = page_size

		def component(name: str, size: int) -> bytes:
			nonlocal position
			end = position + size
			if end > len(raw):
				raise BootImageError(f"{name} extends beyond the image")
			payload = raw[position:end]
			position = align(end, page_size)
			return payload

		kernel = component("kernel", kernel_size)
		ramdisk = component("ramdisk", ramdisk_size)
		second = component("second", second_size)
		dtb = component("dtb", dt_size)

		if position > len(raw):
			raise BootImageError("aligned component area extends beyond the image")

		trailer = raw[position:]
		meaningful_trailer = trailer.rstrip(b"\0")
		if meaningful_trailer not in (b"", SAMSUNG_MARKER):
			raise BootImageError(
				"unsupported data follows the boot components; refusing to discard "
				"a signature or unknown trailer"
			)

		marker = meaningful_trailer
		partition_padding = len(trailer) - len(marker)
		image = cls(
			path=path,
			raw=raw,
			header_page=raw[:page_size],
			kernel=kernel,
			ramdisk=ramdisk,
			second=second,
			dtb=dtb,
			page_size=page_size,
			kernel_address=kernel_address,
			ramdisk_address=ramdisk_address,
			second_address=second_address,
			tags_address=tags_address,
			marker=marker,
			partition_padding=partition_padding,
			stored_id=unpacked[-1],
		)

		calculated_id = image.calculate_id(kernel, dtb)
		if image.stored_id != calculated_id:
			print(
				"warning: base boot image has a stale SHA-1 ID; output ID will be fixed",
				file=sys.stderr,
			)
		return image

	def calculate_id(self, kernel: bytes, dtb: bytes) -> bytes:
		digest = hashlib.sha1()
		for payload in (kernel, self.ramdisk, self.second, dtb):
			digest.update(payload)
			digest.update(struct.pack("<I", len(payload)))
		return digest.digest() + bytes(ID_SIZE - digest.digest_size)

	def repack(self, kernel: bytes, dtb: Optional[bytes] = None) -> bytes:
		if not kernel:
			raise BootImageError("replacement kernel is empty")
		base_format = kernel_format(self.kernel)
		new_format = kernel_format(kernel)
		if new_format != base_format:
			raise BootImageError(
				f"replacement kernel format is {new_format}, but the base uses {base_format}"
			)

		new_dtb = self.dtb if dtb is None else dtb
		if self.dtb and not new_dtb:
			raise BootImageError("replacement DTB is empty")

		header_page = bytearray(self.header_page)
		struct.pack_into("<I", header_page, KERNEL_SIZE_OFFSET, len(kernel))
		struct.pack_into("<I", header_page, DT_SIZE_OFFSET, len(new_dtb))
		header_page[ID_OFFSET:ID_OFFSET + ID_SIZE] = self.calculate_id(kernel, new_dtb)

		output = bytearray(header_page)
		for payload in (kernel, self.ramdisk, self.second, new_dtb):
			output.extend(payload)
			output.extend(bytes(align(len(output), self.page_size) - len(output)))

		output.extend(self.marker)
		if self.partition_padding:
			if len(output) > len(self.raw):
				raise BootImageError(
					f"new image needs {human_size(len(output))}, but the base partition "
					f"image is only {human_size(len(self.raw))}"
				)
			output.extend(bytes(len(self.raw) - len(output)))
		return bytes(output)


def parse_args() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description=(
			"Replace the kernel in a Samsung legacy boot.img while preserving its "
			"ramdisk, DTB, command line, addresses, marker, and partition size."
		)
	)
	parser.add_argument("--boot-image", required=True, type=Path, help="base boot.img")
	parser.add_argument("--kernel", type=Path, help="new Image or Image.gz")
	parser.add_argument("--dtb", type=Path, help="optional replacement dtb.img")
	parser.add_argument("--output", type=Path, help="output boot.img")
	parser.add_argument(
		"--print-kernel-target",
		action="store_true",
		help="print Image or Image.gz to match the base, then exit",
	)
	return parser.parse_args()


def main() -> int:
	args = parse_args()
	try:
		base_path = args.boot_image.resolve(strict=True)
		base = LegacyBootImage.load(base_path)
		if args.print_kernel_target:
			print("Image.gz" if kernel_format(base.kernel) == "gzip" else "Image")
			return 0
		if args.kernel is None or args.output is None:
			raise BootImageError("--kernel and --output are required when repacking")

		kernel_path = args.kernel.resolve(strict=True)
		dtb_path = args.dtb.resolve(strict=True) if args.dtb else None
		output_path = args.output.resolve(strict=False)

		if output_path == base_path:
			raise BootImageError("output must not overwrite the base boot image")

		kernel = kernel_path.read_bytes()
		dtb = dtb_path.read_bytes() if dtb_path else None
		result = base.repack(kernel, dtb)

		output_path.parent.mkdir(parents=True, exist_ok=True)
		with tempfile.NamedTemporaryFile(
			dir=output_path.parent,
			prefix=f".{output_path.name}.",
			delete=False,
		) as temporary:
			temporary.write(result)
			temporary_path = Path(temporary.name)

		try:
			written = LegacyBootImage.load(temporary_path)
			if written.kernel != kernel:
				raise BootImageError("verification failed: output kernel differs")
			if written.ramdisk != base.ramdisk or written.second != base.second:
				raise BootImageError("verification failed: base ramdisk/second changed")
			if written.dtb != (base.dtb if dtb is None else dtb):
				raise BootImageError("verification failed: output DTB differs")
			os.chmod(temporary_path, 0o644)
			os.replace(temporary_path, output_path)
		except Exception:
			temporary_path.unlink(missing_ok=True)
			raise

		print(f"Base boot image: {base_path}")
		print(f"Legacy page size: {base.page_size} bytes")
		print(f"Kernel: {human_size(len(base.kernel))} -> {human_size(len(kernel))}")
		print("DTB: replaced" if dtb is not None else "DTB: preserved from base")
		print(
			"Samsung marker: preserved"
			if base.marker
			else "Samsung marker: not present in base"
		)
		print(f"Output size: {human_size(len(result))}")
		return 0
	except (BootImageError, FileNotFoundError, OSError) as error:
		print(f"error: {error}", file=sys.stderr)
		return 1


if __name__ == "__main__":
	raise SystemExit(main())
