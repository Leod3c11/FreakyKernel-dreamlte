#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0

set -Eeuo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/out"
OUTPUT_IMAGE="$ROOT_DIR/dist/FreakyKernel-dreamlte-boot.img"
DEFCONFIG="exynos8895-dreamlte_defconfig"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')"
BASE_BOOT=""
KERNEL_FILE=""
CROSS_PREFIX="${CROSS_COMPILE:-}"
REPLACE_DTB=0

usage() {
	cat <<'EOF'
Build FreakyKernel for dreamlte and repack a flashable Samsung boot.img.

Usage:
  ./build_boot.sh --boot-image PATH [options]

Required:
  -b, --boot-image PATH       Current boot.img used as the packing base

Options:
  -o, --output PATH           Output image (default: dist/FreakyKernel-dreamlte-boot.img)
      --out-dir PATH          Kernel build directory (default: out)
      --defconfig NAME        Kernel defconfig (default: exynos8895-dreamlte_defconfig)
  -j, --jobs NUMBER           Parallel build jobs (default: available CPUs)
      --cross-compile PREFIX  GCC prefix, ending in aarch64-linux-android-
      --replace-dtb           Also replace the base DTB with the built dtb.img
      --kernel PATH           Repack an existing Image or Image.gz without compiling
  -h, --help                  Show this help

The default operation preserves the base ramdisk, DTB, command line, load
addresses, SEANDROIDENFORCE marker, and partition-sized zero padding. The
kernel compression format is matched to the base. This script intentionally
supports GCC builds only.

Examples:
  ./build_boot.sh -b /path/to/boot.img \
    --cross-compile /path/to/gcc/bin/aarch64-linux-android-

  ./build_boot.sh -b /path/to/boot.img --kernel out/arch/arm64/boot/Image.gz
EOF
}

die() {
	printf 'error: %s\n' "$*" >&2
	exit 1
}

while (($#)); do
	case "$1" in
	-b|--boot-image)
		(($# >= 2)) || die "$1 requires a path"
		BASE_BOOT="$2"
		shift 2
		;;
	-o|--output)
		(($# >= 2)) || die "$1 requires a path"
		OUTPUT_IMAGE="$2"
		shift 2
		;;
	--out-dir)
		(($# >= 2)) || die "$1 requires a path"
		BUILD_DIR="$2"
		shift 2
		;;
	--defconfig)
		(($# >= 2)) || die "$1 requires a name"
		DEFCONFIG="$2"
		shift 2
		;;
	-j|--jobs)
		(($# >= 2)) || die "$1 requires a number"
		JOBS="$2"
		shift 2
		;;
	--cross-compile)
		(($# >= 2)) || die "$1 requires a prefix"
		CROSS_PREFIX="$2"
		shift 2
		;;
	--replace-dtb)
		REPLACE_DTB=1
		shift
		;;
	--kernel)
		(($# >= 2)) || die "$1 requires a path"
		KERNEL_FILE="$2"
		shift 2
		;;
	-h|--help)
		usage
		exit 0
		;;
	--)
		shift
		break
		;;
	*)
		die "unknown option: $1"
		;;
	esac
done

(($# == 0)) || die "unexpected argument: $1"
[[ -n "$BASE_BOOT" ]] || die "--boot-image is required"
[[ -f "$BASE_BOOT" ]] || die "base boot image not found: $BASE_BOOT"
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || die "--jobs must be a positive integer"
command -v python3 >/dev/null 2>&1 || die "python3 is required"

BUILD_DIR="$(realpath -m -- "$BUILD_DIR")"
OUTPUT_IMAGE="$(realpath -m -- "$OUTPUT_IMAGE")"
BASE_BOOT="$(realpath -- "$BASE_BOOT")"
[[ "$OUTPUT_IMAGE" != "$BASE_BOOT" ]] || die "output must not overwrite the base boot image"

find_cross_prefix() {
	local gcc_path
	local candidate

	if [[ -n "$CROSS_PREFIX" ]]; then
		command -v "${CROSS_PREFIX}gcc" >/dev/null 2>&1 ||
			die "GCC not found at ${CROSS_PREFIX}gcc"
		return
	fi

	for candidate in \
		"$ROOT_DIR/toolchain/bin/aarch64-linux-android-" \
		"$ROOT_DIR/toolchain/gcc_4.9/bin/aarch64-linux-android-"; do
		if [[ -x "${candidate}gcc" ]]; then
			CROSS_PREFIX="$candidate"
			return
		fi
	done

	if gcc_path="$(command -v aarch64-linux-android-gcc 2>/dev/null)"; then
		CROSS_PREFIX="${gcc_path%gcc}"
		return
	fi

	die "aarch64 GCC toolchain not found; pass --cross-compile /path/to/aarch64-linux-android-"
}

REPACKER="$ROOT_DIR/scripts/shark/repack_bootimg.py"

if [[ -z "$KERNEL_FILE" ]]; then
	KERNEL_TARGET="$(python3 "$REPACKER" \
		--boot-image "$BASE_BOOT" \
		--print-kernel-target)"
	find_cross_prefix
	mkdir -p -- "$BUILD_DIR"

	printf 'Configuring %s with GCC...\n' "$DEFCONFIG"
	make -C "$ROOT_DIR" \
		O="$BUILD_DIR" \
		ARCH=arm64 \
		CROSS_COMPILE="$CROSS_PREFIX" \
		"$DEFCONFIG"

	printf 'Building %s and dtb.img with %s jobs...\n' "$KERNEL_TARGET" "$JOBS"
	make -C "$ROOT_DIR" \
		O="$BUILD_DIR" \
		ARCH=arm64 \
		CROSS_COMPILE="$CROSS_PREFIX" \
		-j"$JOBS" \
		"$KERNEL_TARGET" dtb.img

	KERNEL_FILE="$BUILD_DIR/arch/arm64/boot/$KERNEL_TARGET"
else
	KERNEL_FILE="$(realpath -- "$KERNEL_FILE")"
	printf 'Skipping compilation; using %s\n' "$KERNEL_FILE"
fi

[[ -s "$KERNEL_FILE" ]] || die "kernel image not found or empty: $KERNEL_FILE"

repack_args=(
	"$REPACKER"
	--boot-image "$BASE_BOOT"
	--kernel "$KERNEL_FILE"
	--output "$OUTPUT_IMAGE"
)

if ((REPLACE_DTB)); then
	dtb_file="$BUILD_DIR/arch/arm64/boot/dtb.img"
	[[ -s "$dtb_file" ]] || die "--replace-dtb requested, but dtb.img was not found: $dtb_file"
	repack_args+=(--dtb "$dtb_file")
fi

mkdir -p -- "$(dirname -- "$OUTPUT_IMAGE")"
python3 "${repack_args[@]}"

printf '\nFlashable boot image: %s\n' "$OUTPUT_IMAGE"
printf 'SHA-256: '
sha256sum "$OUTPUT_IMAGE" | awk '{print $1}'
