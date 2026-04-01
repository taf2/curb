#!/usr/bin/env bash
set -euo pipefail

is_sourced=0
if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  is_sourced=1
fi

die() {
  echo "switch.sh: $*" >&2
  if ((is_sourced)); then
    return 1
  fi
  exit 1
}

usage() {
  cat <<'EOF'
Usage: source ./switch.sh <ruby-version>
       ./switch.sh <ruby-version>

Switch to the requested Ruby version, clean vendor/bundle, and reinstall gems.
EOF
  if ((is_sourced)); then
    return 0
  fi
  exit 0
}

if [[ $# -lt 1 || "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
fi

ruby_version="$1"

run_with_nounset_disabled() {
  local had_nounset=0
  case $- in
    *u*) had_nounset=1 ;;
  esac

  set +u
  "$@"
  local status=$?

  if ((had_nounset)); then
    set -u
  fi

  return "$status"
}

if ! command -v rvm >/dev/null 2>&1; then
  if [[ -s "$HOME/.rvm/scripts/rvm" ]]; then
    # shellcheck source=/dev/null
    run_with_nounset_disabled source "$HOME/.rvm/scripts/rvm"
  elif [[ -s "/etc/profile.d/rvm.sh" ]]; then
    # shellcheck source=/dev/null
    run_with_nounset_disabled source "/etc/profile.d/rvm.sh"
  else
    die "rvm not found; run inside an RVM shell or install RVM."
  fi
fi

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$root_dir"

if [[ ! -f "Gemfile" ]]; then
  die "Gemfile not found in $root_dir."
fi

run_with_nounset_disabled rvm use "$ruby_version" --install

bundler_version=""
if [[ -f "Gemfile.lock" ]]; then
  bundler_version="$(awk 'found && NF {print $1; exit} /BUNDLED WITH/{found=1}' Gemfile.lock)"
fi

cpu_count() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu 2>/dev/null || echo 1
  else
    echo 1
  fi
}

jobs="$(cpu_count)"

bundle_cmd=(bundle)
if [[ -n "$bundler_version" ]]; then
  if ! gem list -i bundler -v "$bundler_version" >/dev/null 2>&1; then
    gem install bundler -v "$bundler_version"
  fi
  bundle_cmd=(bundle "_${bundler_version}_")
fi

rm -rf vendor/bundle

"${bundle_cmd[@]}" config set --local path vendor/bundle
if [[ -n "$jobs" && "$jobs" -gt 1 ]]; then
  "${bundle_cmd[@]}" config set --local jobs "$jobs"
fi

MAKEFLAGS="-j${jobs}" BUNDLE_FORCE_RUBY_PLATFORM=true "${bundle_cmd[@]}" install
MAKEFLAGS="-j${jobs}" "${bundle_cmd[@]}" pristine json

echo "Ruby: $(ruby -v)"
"${bundle_cmd[@]}" exec ruby -e 'require "json"; puts "JSON: #{JSON::VERSION}"; p JSON::Ext::ParserConfig'

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "Note: run 'source ./switch.sh $ruby_version' to keep the Ruby selection in your current shell."
fi
