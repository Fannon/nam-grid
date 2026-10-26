#!/usr/bin/env bash
# Build native CLAP and VST3 plugins from Git Bash, WSL, macOS or Linux.
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: bash build.sh [--setup] [--test] [--jobs N]

Build Release CLAP and VST3 plugins. Dependencies are prepared on the first
run; use --setup again after pulling dependency or patch changes.

  --setup     Refresh pinned submodules, SDKs and dependency patches
  --test      Build and run CTest and Python tests as well
  --jobs N    Compile up to N files in parallel (default: 3)
  -h, --help  Show this help

Install the platform prerequisites in docs/build.md first.
Git Bash produces Windows plugins; WSL produces Linux plugins.
For cross-platform release builds using GitHub Actions, see docs/build.md:
  gh workflow run release-native.yml -R Fannon/nam-grid -f tag=v<VERSION>
This builds the existing tag on all platforms and creates/updates release assets.
macOS Apple Silicon bundles are ad-hoc signed, not notarized.
EOF
}

setup=0
tests=0
jobs=3
while (( $# )); do
  case "$1" in
    --setup) setup=1; shift ;;
    --test) tests=1; shift ;;
    --jobs)
      if [[ ${2:-} =~ ^[1-9][0-9]*$ ]]; then
        jobs=$2; shift 2
      else
        echo 'Error: --jobs requires a positive integer.' >&2; exit 2
      fi ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Error: unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
case "$(uname -s)" in
  MINGW*|MSYS*) platform=windows ;;
  Darwin) platform=macos ;;
  Linux) platform=linux ;;
  *) echo 'Error: supported platforms are Windows (Git Bash), macOS and Linux.' >&2; exit 1 ;;
esac
build_dir="build/$platform/release"

# The CMake installer may not have been on PATH when this terminal was opened.
if [[ $platform == windows ]] && ! command -v cmake >/dev/null 2>&1; then
  cmake_bin="${PROGRAMFILES:-C:/Program Files}/CMake/bin"
  cmake_bin=$(cygpath -u "$cmake_bin")
  if [[ -x "$cmake_bin/cmake.exe" ]]; then
    export PATH="$cmake_bin:$PATH"
  fi
fi

for tool in git cmake; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Error: $tool is missing from PATH. See docs/build.md for prerequisites." >&2
    if [[ $platform == windows && $tool == cmake ]]; then
      echo 'Install from PowerShell: winget install --id Kitware.CMake --exact' >&2
      echo 'Then reopen your terminal and run: bash build.sh' >&2
    fi
    exit 1
  fi
done
if [[ $tests == 1 ]]; then
  if ! command -v ctest >/dev/null 2>&1; then
    echo 'Error: ctest is missing from PATH (install CMake).' >&2; exit 1
  fi
  if [[ $platform == linux && -z ${DISPLAY:-} ]] && ! command -v xvfb-run >/dev/null 2>&1; then
    echo 'Error: headless Linux tests require xvfb-run. See docs/build.md.' >&2; exit 1
  fi
fi
python_cmd=
for candidate in python python3; do
  if command -v "$candidate" >/dev/null 2>&1 &&
     "$candidate" -c 'import sys; sys.exit(sys.version_info < (3, 8))' >/dev/null 2>&1; then
    python_cmd=$candidate
    break
  fi
done
if [[ -z $python_cmd ]]; then
  echo 'Error: Python 3.8 or newer is required on PATH. See docs/build.md.' >&2
  exit 1
fi

if [[ $setup == 1 || ! -f "$build_dir/.deps-ready" ]]; then
  echo 'Preparing pinned dependencies...'
  mkdir -p "$build_dir"
  rm -f "$build_dir/.deps-ready"
  git submodule sync --recursive
  git -c 'url.https://github.com/eigen-mirror/eigen.insteadOf=https://gitlab.com/libeigen/eigen' \
    submodule update --init --recursive
  "$python_cmd" scripts/setup-deps.py
  touch "$build_dir/.deps-ready"
fi

generator=()
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  if [[ $platform == windows ]]; then
    if [[ -n ${CMAKE_GENERATOR:-} ]]; then
      generator=(-G "$CMAKE_GENERATOR")
    elif command -v cl >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
      generator=(-G Ninja)
    else
      generator=(-G 'Visual Studio 17 2022' -A x64)
    fi
  elif command -v ninja >/dev/null 2>&1; then
    generator=(-G Ninja)
  fi
fi
cmake -S . -B "$build_dir" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release '-DNAM_FORMATS=CLAP;VST3'
targets=(nam-grid-clap nam-grid-vst3)
if [[ $tests == 1 ]]; then
  targets+=(nam-stage-tests nam-aliasing-tests nam-clap-smoke nam-tuner-tests nam-parameter-contract
    nam-library-tests nam-wav-tests nam-resampling-tests nam-ir-tests)
  if [[ $platform == windows ]]; then
    targets+=(nam-vst3-parameter-contract)
  fi
fi
cmake --build "$build_dir" --config Release --target "${targets[@]}" --parallel "$jobs"
if [[ $tests == 1 ]]; then
  if [[ $platform == linux && -z ${DISPLAY:-} ]]; then
    xvfb-run -a -s '-screen 0 1920x1080x24' ctest --test-dir "$build_dir" -C Release --output-on-failure
  else
    ctest --test-dir "$build_dir" -C Release --output-on-failure
  fi
  "$python_cmd" -m unittest discover -s tests -p 'test_*.py'
fi
printf '\nBuild complete. CLAP and VST3 plugins: %s/out/\n' "$build_dir"
