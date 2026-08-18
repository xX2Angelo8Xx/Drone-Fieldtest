#!/usr/bin/env bash
set -euo pipefail

# Publish a lightweight build-progress snapshot back to the Speech Notes branch.
# All commits use [skip ci], so these telemetry pushes do not recursively start
# or cancel the actual build workflow.
#
# Usage:
#   .github/speechnotes-progress.sh <phase> <state> [detail] [log_file]

PHASE="${1:-unknown}"
STATE="${2:-running}"
DETAIL="${3:-}"
LOG_FILE="${4:-}"
PROGRESS_FILE=".github/speechnotes-build-progress.txt"
BRANCH="chatgpt-whisper-dictation-v1"

mkdir -p .github
{
  echo "source_commit=${SPEECHNOTES_SOURCE_SHA:-${GITHUB_SHA:-unknown}}"
  echo "run_id=${GITHUB_RUN_ID:-unknown}"
  echo "run_attempt=${GITHUB_RUN_ATTEMPT:-unknown}"
  echo "phase=$PHASE"
  echo "state=$STATE"
  echo "updated_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "detail=$DETAIL"
  if [ -n "$LOG_FILE" ] && [ -f "$LOG_FILE" ]; then
    echo
    echo "===== LIVE LOG TAIL ====="
    tail -n 35 "$LOG_FILE" || true
  fi
} > "$PROGRESS_FILE"

git config user.name "github-actions[bot]"
git config user.email "41898282+github-actions[bot]@users.noreply.github.com"
git add "$PROGRESS_FILE"
if git diff --cached --quiet; then
  exit 0
fi

git commit -m "[skip ci] Speech Notes progress: $PHASE $STATE" >/dev/null

# The build itself is the only writer during a run. Retry transient push races
# (for example a previous skipped telemetry event becoming visible remotely).
for attempt in 1 2 3 4; do
  if git push origin HEAD:"$BRANCH" >/dev/null 2>&1; then
    exit 0
  fi
  sleep $((attempt * 2))
done

echo "Warning: could not publish progress snapshot after retries" >&2
exit 0
