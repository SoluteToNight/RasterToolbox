#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

timestamp_utc="$(date -u +%Y%m%dT%H%M%SZ)"
evidence_file=".omx/plans/phase6-real-data-evidence-${timestamp_utc}.md"
mkdir -p .omx/plans

log_command() {
  local command="$1"
  local output

  echo "\$ ${command}" | tee -a "${evidence_file}"
  if ! output=$(bash -lc "${command}" 2>&1); then
    echo "${output}" | tee -a "${evidence_file}"
    echo "" | tee -a "${evidence_file}"
    echo "❌ 命令失败，已中止。" | tee -a "${evidence_file}"
    exit 1
  fi
  echo "${output}" | tee -a "${evidence_file}"
  echo "" | tee -a "${evidence_file}"
}

cat > "${evidence_file}" <<EOF
# Phase 6 一键验收证据（${timestamp_utc}）

## 自动命令执行记录

EOF

log_command "pixi run configure"
log_command "pixi run build"
log_command "pixi run test"
log_command "QT_QPA_PLATFORM=offscreen pixi run ./build/dev/rastertoolbox --smoke-startup"

phase6_output="$(bash -lc "pixi run ./build/dev/tests/test_real_data_phase6" 2>&1)"
echo "\$ pixi run ./build/dev/tests/test_real_data_phase6" | tee -a "${evidence_file}"
echo "${phase6_output}" | tee -a "${evidence_file}"
echo "" | tee -a "${evidence_file}"

tif_path="$(printf '%s\n' "${phase6_output}" | sed -n 's/^tif-output="\(.*\)"/\1/p')"
gpkg_path="$(printf '%s\n' "${phase6_output}" | sed -n 's/^gpkg-output="\(.*\)"/\1/p')"
png_path="$(printf '%s\n' "${phase6_output}" | sed -n 's/^png-output="\(.*\)"/\1/p')"

if [[ -z "${tif_path}" || -z "${gpkg_path}" || -z "${png_path}" ]]; then
  echo "❌ 无法从 phase6 测试输出中提取输出路径。" | tee -a "${evidence_file}"
  exit 1
fi

if [[ ! -f "${tif_path}" || ! -f "${gpkg_path}" || ! -f "${png_path}" ]]; then
  echo "❌ phase6 测试输出文件不存在。" | tee -a "${evidence_file}"
  exit 1
fi

log_command "pixi run gdalinfo \"${tif_path}\""
log_command "pixi run gdalinfo \"${gpkg_path}\""
log_command "pixi run gdalinfo \"${png_path}\""

cat >> "${evidence_file}" <<EOF
## 结论

- ✅ 全量回归通过
- ✅ phase6 真实样本链路通过
- ✅ 三份输出影像 gdalinfo 可读

EOF

echo "✅ Phase 6 验收完成，证据文件：${evidence_file}"
