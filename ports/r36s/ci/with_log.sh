#!/bin/bash
# Runs a CI step under strict mode with its output teed to a log. On failure
# the head and tail are re-emitted as ::error:: workflow commands, because CI
# logs need a login to read while annotations do not.
#
# `set -e` is inert inside a subshell that feeds a pipeline, so the step runs
# as a separate bash process with errexit set at startup.
#
# Usage: with_log.sh <log-name> <command> [args...]
set -o pipefail

name=$1
shift
mkdir -p build
log="build/$name.log"

if ! bash -euxo pipefail "$@" 2>&1 | tee "$log"; then
  head -n 40 "$log" | sed 's/^/::error::/' || true
  echo "::error::... ($(wc -l < "$log") lines, tail below)"
  tail -n 20 "$log" | sed 's/^/::error::/' || true
  exit 1
fi
