# Local GitHub Actions With act

This directory contains one wrapper for running the repository's GitHub Actions workflows locally with [`act`](https://github.com/nektos/act).

```sh
scripts/act/act.sh <command> [act args...]
```

## Install act

Official documentation:

- act repository: https://github.com/nektos/act
- installation guide: https://nektosact.com/installation/index.html
- usage guide: https://nektosact.com/usage/index.html

`act` needs:

- Git
- a container runtime, usually Docker Engine or Docker Desktop
- access to the Docker socket or equivalent container runtime permissions
- enough disk space for runner images and build artifacts

On Linux, install Docker through your distribution or Docker's official packages, then install `act` using one of the official methods. For example, using the install script from the act documentation:

```sh
curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
```

On macOS, Docker Desktop or Colima is commonly used as the container runtime. On Windows, Docker Desktop with WSL2 is the usual path.

## Commands

Run one workflow:

```sh
scripts/act/act.sh cmake
scripts/act/act.sh lint
scripts/act/act.sh linux-pack
scripts/act/act.sh windows-pack
```

Run all workflows in series:

```sh
scripts/act/act.sh all-series
```

List workflow jobs without running them:

```sh
scripts/act/act.sh list
```

Validate a workflow without creating containers:

```sh
scripts/act/act.sh dry-run cmake
scripts/act/act.sh dry-run all-series
```

Pass extra arguments directly to `act`:

```sh
scripts/act/act.sh cmake --container-architecture linux/amd64
```

The wrapper maps `ubuntu-24.04` to `catthehacker/ubuntu:act-24.04` by default
so `act` does not stop at its first-run image prompt. Override that mapping
when needed:

```sh
ACT_PLATFORM=ubuntu-24.04=ghcr.io/catthehacker/ubuntu:act-latest scripts/act/act.sh cmake
```

## Parallel Runs

GitHub-hosted workflows run in isolated workspaces, so independent workflows can run in parallel there.

Local `act` runs use the current checkout by default. Running every workflow in parallel can race on shared files such as `dist/` and generated artifacts. Because of that, `all-parallel` refuses to run unless explicitly enabled:

```sh
ACT_ALLOW_SHARED_WORKTREE=1 scripts/act/act.sh all-parallel
```

Use `all-series` for the safe local default.

## Useful act Features

The wrapper keeps the common commands short, but most `act` flags can still be
passed through after the workflow name:

- `--dryrun` validates workflow structure without starting containers. The
  wrapper exposes this as `scripts/act/act.sh dry-run <workflow>`.
- `--list` lists jobs, and `--graph` prints the workflow dependency graph.
- `--job <name>` runs a single job from a workflow.
- `--matrix key:value` narrows a matrix run.
- `--platform <runner=image>` maps GitHub runner labels to local images.
- `--container-architecture linux/amd64` is useful on non-amd64 hosts.
- `--eventpath <file>` supplies a local event payload.
- `--input name=value` and `--input-file <file>` supply workflow-dispatch
  inputs.
- `--env`, `--env-file`, `--secret`, `--secret-file`, `--var`, and
  `--var-file` mirror GitHub Actions runtime data locally.
- `--artifact-server-path` and `--cache-server-path` keep artifacts and cache
  data in predictable local directories.
- `--action-offline-mode` reuses cached actions when testing without network
  access.
- `--watch` reruns workflows when files change.

See the official usage guide for the full option set:
https://nektosact.com/usage/index.html

## Windows Workflow

`windows-pack.yml` targets `windows-2022`. `act` does not provide a real Windows GitHub-hosted runner on a Linux machine. The wrapper can list the workflow and preserve the command shape, but actually running the Windows package workflow locally needs an appropriate platform mapping or a Windows/self-hosted runner setup.
