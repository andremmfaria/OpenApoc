#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
act_bin=${ACT_BIN:-act}

usage() {
  cat <<'USAGE'
usage: scripts/act/act.sh <command> [act args...]

commands:
  cmake          run .github/workflows/cmake.yml as push
  hygiene        run .github/workflows/hygiene.yml as pull_request
  lint           run .github/workflows/lint.yml as push
  linux-pack     run .github/workflows/linux-pack.yml as push
  windows-pack   run .github/workflows/windows-pack.yml as push
  all-series     run every workflow wrapper one after another
  all-parallel   run every workflow wrapper concurrently in this checkout
  dry-run NAME   validate a workflow without creating containers
  list           list all workflow jobs through act

environment:
  ACT_BIN                       act binary path, defaults to "act"
  ACT_LOG_DIR                   log directory for all-parallel, defaults to .act/logs
  ACT_PLATFORM                  act runner image mapping, defaults to ubuntu-24.04=catthehacker/ubuntu:act-24.04
  ACT_ALLOW_SHARED_WORKTREE=1   required for all-parallel
USAGE
}

need_act() {
  if ! command -v "$act_bin" >/dev/null 2>&1; then
    echo "act not found. Install act or set ACT_BIN=/path/to/act." >&2
    exit 127
  fi
}

run_workflow() {
  event=$1
  workflow=$2
  shift 2
  need_act
  cd "$repo_root"
  act_platform=${ACT_PLATFORM:-ubuntu-24.04=catthehacker/ubuntu:act-24.04}
  if [ -n "$act_platform" ]; then
    exec "$act_bin" "$event" -P "$act_platform" -W ".github/workflows/$workflow" "$@"
  fi
  exec "$act_bin" "$event" -W ".github/workflows/$workflow" "$@"
}

run_named() {
  name=$1
  shift
  case "$name" in
    cmake)
      run_workflow push cmake.yml "$@"
      ;;
    hygiene)
      run_workflow pull_request hygiene.yml "$@"
      ;;
    lint)
      run_workflow push lint.yml "$@"
      ;;
    linux-pack)
      run_workflow push linux-pack.yml "$@"
      ;;
    windows-pack)
      run_workflow push windows-pack.yml "$@"
      ;;
    *)
      echo "unknown workflow command: $name" >&2
      usage >&2
      exit 2
      ;;
  esac
}

run_series() {
  run_named cmake "$@"
  run_named hygiene "$@"
  run_named lint "$@"
  run_named linux-pack "$@"
  run_named windows-pack "$@"
}

run_parallel() {
  if [ "${ACT_ALLOW_SHARED_WORKTREE:-}" != "1" ]; then
    cat >&2 <<'WARNING'
Refusing to run all workflows in parallel against one shared checkout.

GitHub-hosted workflows run in isolated workspaces. Local act runs share this
checkout by default, so parallel runs can race on files such as dist/ and
workflow artifacts.

Set ACT_ALLOW_SHARED_WORKTREE=1 if you explicitly accept that risk.
WARNING
    exit 2
  fi

  log_dir=${ACT_LOG_DIR:-"$repo_root/.act/logs"}
  mkdir -p "$log_dir"
  workflows="cmake hygiene lint linux-pack windows-pack"
  pids=""
  failed=0

  for workflow in $workflows; do
    log_file="$log_dir/$workflow.log"
    echo "starting $workflow -> $log_file"
    "$0" "$workflow" "$@" >"$log_file" 2>&1 &
    pids="$pids $!:$workflow:$log_file"
  done

  for item in $pids; do
    pid=${item%%:*}
    rest=${item#*:}
    workflow=${rest%%:*}
    log_file=${rest#*:}
    if wait "$pid"; then
      echo "passed $workflow"
    else
      echo "failed $workflow -> $log_file" >&2
      failed=1
    fi
  done

  exit "$failed"
}

run_dry() {
  if [ "$#" -lt 1 ]; then
    echo "dry-run needs a workflow command" >&2
    usage >&2
    exit 2
  fi

  target=$1
  shift
  case "$target" in
    cmake|hygiene|lint|linux-pack|windows-pack)
      run_named "$target" --dryrun "$@"
      ;;
    all-series)
      run_series --dryrun "$@"
      ;;
    *)
      echo "unknown dry-run target: $target" >&2
      usage >&2
      exit 2
      ;;
  esac
}

list_workflows() {
  need_act
  cd "$repo_root"
  "$0" cmake --list
  "$0" hygiene --list
  "$0" lint --list
  "$0" linux-pack --list
  "$0" windows-pack --list
}

if [ "$#" -lt 1 ]; then
  usage >&2
  exit 2
fi

command=$1
shift

case "$command" in
  cmake|hygiene|lint|linux-pack|windows-pack)
    run_named "$command" "$@"
    ;;
  all-series)
    run_series "$@"
    ;;
  all-parallel)
    run_parallel "$@"
    ;;
  dry-run)
    run_dry "$@"
    ;;
  list)
    list_workflows "$@"
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    echo "unknown command: $command" >&2
    usage >&2
    exit 2
    ;;
esac
