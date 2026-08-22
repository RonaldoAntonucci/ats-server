# Native macOS development

The supported native workflow targets Apple Silicon. It compiles and runs the
C++ test binaries on macOS without Docker. Database, authentication, and other
services still run only in Docker.

## Authoritative configuration

Use these repository files as the only setup sources:

- `.mise.toml` pins CMake, Ninja, sccache, and Python and defines the commands.
- `CMakePresets.json` defines `macos-debug` and `macos-release`.
- `vcpkg.json` and its configured registry define C++ dependencies.

Do not install MariaDB or another service natively. The integration suite uses
the disposable database from `tests/docker-compose.yaml`. Before starting it,
reuse its image when available:

```sh
docker image inspect tests-otdb-test
docker compose -f tests/docker-compose.yaml up -d otdb-test
```

Build that image only when `docker image inspect tests-otdb-test` proves it is
absent. Docker remains the runtime and Linux-parity path.

## Toolchain consent

Installing a missing macOS dependency requires one explicit approval that
names its function, necessity, and destination. Approval for one dependency
does not authorize the next dependency. Downloading dependencies already
approved by the user follows the pinned manifests above.

## Commands

From the repository root:

```sh
mise run configure macos-debug
mise run build macos-debug
mise run test macos-debug
mise run configure macos-release
mise run build macos-release
```

`mise run test macos-debug` expects the disposable Docker database to be
available at the address in `tests/test.env`. Stop the service when it is no
longer needed:

```sh
docker compose -f tests/docker-compose.yaml down
```

## Baseline evidence

Validated on Apple Silicon on 2026-08-22 before Armamento Assault feature code:

| Assertion | Command | Result |
| --- | --- | --- |
| Debug configure | `mise run configure macos-debug` | exit `0` |
| Debug build | `mise run build macos-debug` | exit `0`, 282 targets built |
| Native tests | `mise run test macos-debug` | exit `0`, 479/479 tests passed |
| Release configure and build | `mise run configure macos-release` then `mise run build macos-release` | both exit `0`, 34 targets built |
| Docker runtime | `tests/runtime/discipline_startup_test.sh` with its unconditional image build replaced by a successful inspect of existing `ats-server:disciplines-runtime` | exit `0`, valid and invalid Discipline catalog checks passed |

The Armamento Assault Lua test does not exist at this baseline and is not
reported as passed. It becomes mandatory in the Phase 2 full gate.
