#!/bin/sh
# Clone the upstream projects this port builds on into refs/, at pinned commits. refs/ is read-only reference
# material and ignored by git. Adapted from totk-pc-recomp tools/fetch_refs.sh, by way of fe3h-pc-recomp.
# metroid-prime-4-recomp is the direct predecessor and the source of this tree; fe3h-pc-recomp and totk-pc-recomp
# before it record which parts of a Switch recomp turned out to be game independent.
set -eu

cd "$(dirname "$0")/.."
mkdir -p refs

fetch() {
    name=$1 url=$2 commit=$3
    dir=refs/$name
    if [ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" = "$commit" ]; then
        echo "$name: up to date"
        return
    fi
    rm -rf "$dir"
    git init -q "$dir"
    git -C "$dir" remote add origin "$url"
    git -C "$dir" fetch -q --depth 1 origin "$commit"
    git -C "$dir" checkout -q FETCH_HEAD
    echo "$name: $commit"
}

fetch metroid-prime-4-recomp https://github.com/jaigner-hub/metroid-prime-4-recomp.git 586e91e1b6f38c9e512e39f2d47a046fdf273cf0
fetch fe3h-pc-recomp https://github.com/jaigner-hub/fe3h-pc-recomp.git 47ef99126793dbd13e4a475feb910a13ef6639b8
fetch totk-pc-recomp https://github.com/valentderah/totk-pc-recomp.git c944f9f94520874f2a669add30dd08d29ee9f7b5
fetch LASRecomp      https://github.com/peiikai/LASRecomp.git        da35b83a9611d046a9a681aae668d7085954d37f
fetch MacNativeGames https://github.com/ly05010419/MacNativeGames.git b75be8cfa704df3701e86130c8ced268e79d6b11
fetch nxnative       https://github.com/seedds/nxnative.git          d5b380b3a80495609c2e751bb994911a638935ad
fetch SwitchRecomp   https://github.com/bgyss/SwitchRecomp.git       0f99e1c5c1e78fdc8e01e58a0f8dc8622e82976b
