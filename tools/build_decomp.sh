#!/usr/bin/env bash
# Builds the decomp's US version (lib/papermario's commit) from your ROM and brings its ELF back for tools/gen_syms.sh.
# It runs in tools/docker/Dockerfile.decomp's image with Docker, here or on a Linux host over ssh (BUILD_HOST). Never
# in CI: the build needs the ROM.
#
# `./configure us` splits the ROM's code and assets with splat, and ninja builds them with the decomp's compilers
# (install_compilers.sh) and checks the ROM it links against ver/us/checksum.sha1. Into build/decomp/: papermario.elf.
# Nothing from it is committed but what tools/gen_syms.sh writes from it.
#
# Usage: [BUILD_HOST=<ssh host>] [DECOMP_DIR=<folder under ~, default papermario-decomp>] tools/build_decomp.sh [roms/papermario.us.z64]
set -euo pipefail
cd "$(dirname "$0")/.."
rom="${1:-roms/papermario.us.z64}"
sha1="3837f44cda784b466c9a2d99df70d77c322b97a0"
[[ "$(sha1sum "$rom" | cut -d' ' -f1)" == "$sha1" ]] || { echo "$rom isn't Paper Mario (USA) as a .z64" >&2; exit 1; }
commit="$(git ls-tree HEAD lib/papermario | awk '{print $3}')"
image="papermario-decomp:$(git hash-object tools/docker/Dockerfile.decomp | cut -c1-12)"
work="${DECOMP_DIR:-papermario-decomp}/${commit:0:12}"

if [[ -n "${BUILD_HOST:-}" ]]; then
    run() { ssh "$BUILD_HOST" "cd && $1"; }
    put() { scp -q "$1" "$BUILD_HOST:$2"; }
    get() { scp -q "$BUILD_HOST:$1" "$2"; }
else
    run() { (cd ~ && bash -c "$1"); }
    put() { cp "$1" ~/"$2"; }
    get() { cp ~/"$1" "$2"; }
fi

run "mkdir -p $work.docker"
put tools/docker/Dockerfile.decomp "$work.docker/Dockerfile"
put lib/papermario/tools/configure/requirements.txt "$work.docker/requirements.txt"
run "docker image inspect $image > /dev/null 2>&1 || docker build -q -t $image $work.docker > /dev/null"
run "[ -d $work/.git ] || git clone -q https://github.com/pmret/papermario $work
     git -C $work checkout -q $commit && git -C $work submodule update -q --init --recursive"
put "$rom" "$work/ver/us/baserom.z64"
run "docker run --rm --name papermario-decomp-${commit:0:12} -u \$(id -u):\$(id -g) -e HOME=/tmp -v \$HOME/$work:/pm -w /pm $image bash -c '
    set -e
    [ -x tools/build/cc/gcc/gcc ] || bash install_compilers.sh > compilers.log 2>&1 || { tail -30 compilers.log; exit 1; }
    ./configure us > configure.log 2>&1 || { tail -30 configure.log; exit 1; }
    ninja > build.log 2>&1 || { tail -30 build.log; exit 1; }
    sha1sum -c ver/us/checksum.sha1'"

mkdir -p build/decomp
get "$work/ver/us/build/papermario.elf" build/decomp/
echo "build/decomp/: the decomp's ELF at ${commit:0:12}, matching the ROM; next, tools/gen_syms.sh"
