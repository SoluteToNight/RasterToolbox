#!/usr/bin/env bash
set -euo pipefail

tag="${1:-${TAG:-}}"
if [[ -z "${tag}" ]]; then
    echo "usage: $0 <tag> (or set TAG env var)" >&2
    exit 1
fi

build_dir="${BUILD_DIR:-build/ci-windows}"
exe_src="${build_dir}/rastertoolbox.exe"
dist_root="dist/windows-release"
zip_path="dist/RasterToolbox-${tag}-windows-x64.zip"

for cmd in windeployqt ntldd zip; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "missing required command: ${cmd}" >&2
        exit 1
    fi
done

if [[ ! -f "${exe_src}" ]]; then
    echo "missing executable: ${exe_src}" >&2
    exit 1
fi

if [[ ! -d "/mingw64/share/gdal" ]]; then
    echo "missing gdal data directory: /mingw64/share/gdal" >&2
    exit 1
fi

if [[ ! -d "/mingw64/share/proj" ]]; then
    echo "missing proj data directory: /mingw64/share/proj" >&2
    exit 1
fi

rm -rf "${dist_root}"
mkdir -p "${dist_root}/share"
rm -f "${zip_path}"

cp "${exe_src}" "${dist_root}/rastertoolbox.exe"

pushd "${dist_root}" >/dev/null
windeployqt --release rastertoolbox.exe
popd >/dev/null

mapfile -t ntdld_tokens < <(ntldd -R "${dist_root}/rastertoolbox.exe" | tr -d '\r' | awk '{ for (i = 1; i <= NF; ++i) print $i }')

declare -a dlls=()
for token in "${ntdld_tokens[@]:-}"; do
    clean="${token//[$'\t\r\n()']/}"
    if [[ "${clean}" =~ ^/mingw64/bin/.*\.[dD][lL][lL]$ ]]; then
        dlls+=("${clean}")
        continue
    fi
    if [[ "${clean}" =~ ^[A-Za-z]:\\.*\\mingw64\\bin\\.*\.[dD][lL][lL]$ ]]; then
        unix_path="$(cygpath -u "${clean}" 2>/dev/null || true)"
        if [[ "${unix_path}" =~ ^/mingw64/bin/.*\.[dD][lL][lL]$ ]]; then
            dlls+=("${unix_path}")
        fi
    fi
done

if (( ${#dlls[@]} == 0 )); then
    echo "ntldd did not report any /mingw64/bin dll dependencies" >&2
    exit 1
fi

mapfile -t dlls < <(printf '%s\n' "${dlls[@]}" | sort -u)
for dll in "${dlls[@]}"; do
    cp -f "${dll}" "${dist_root}/$(basename "${dll}")"
done

cp -a /mingw64/share/gdal "${dist_root}/share/gdal"
cp -a /mingw64/share/proj "${dist_root}/share/proj"

(
    cd "${dist_root}"
    zip -r "../$(basename "${zip_path}")" .
)

if [[ ! -f "${zip_path}" ]]; then
    echo "failed to create release archive: ${zip_path}" >&2
    exit 1
fi

echo "packaged ${zip_path}"
