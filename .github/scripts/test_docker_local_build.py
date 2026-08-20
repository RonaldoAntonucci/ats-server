#!/usr/bin/env python3

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DOCKERFILE = REPO_ROOT / "docker" / "Dockerfile"
BASE_COMPOSE = REPO_ROOT / "docker" / "docker-compose.yml"
LOCAL_COMPOSE = REPO_ROOT / "docker" / "docker-compose.local.yml"
LOCAL_BUILD_COMPOSE = REPO_ROOT / "docker" / "docker-compose.local-build.yml"
LOCAL_AUTH_COMPOSE = REPO_ROOT / "docker" / "docker-compose.local-auth.yml"
SHELL_LAUNCHER = REPO_ROOT / "docker" / "up-local.sh"


class CanonicalDockerfileContractTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls) -> None:
		cls.content = DOCKERFILE.read_text(encoding="utf-8")

	def test_amd64_maps_to_x64_linux(self) -> None:
		self.assertRegex(self.content, r"amd64\)\s+VCPKG_TRIPLET=x64-linux")

	def test_arm64_maps_to_arm64_linux(self) -> None:
		self.assertRegex(self.content, r"arm64\)\s+VCPKG_TRIPLET=arm64-linux")

	def test_unsupported_architecture_fails_explicitly(self) -> None:
		self.assertIn('Unsupported TARGETARCH: ${TARGETARCH}', self.content)
		self.assertRegex(self.content, r"\*\)\s+echo .*Unsupported TARGETARCH.*exit 1")
		self.assertLess(
			self.content.index('Unsupported TARGETARCH: ${TARGETARCH}'),
			self.content.index("apt-get update"),
		)

	def test_no_token_branch_uses_local_file_cache(self) -> None:
		self.assertIn("clear;files,/root/.cache/vcpkg/archives,readwrite", self.content)
		self.assertIn("target=/root/.cache/vcpkg", self.content)

	def test_token_branch_uses_temporary_nuget_config(self) -> None:
		self.assertIn("if [ -s /run/secrets/github_token ]", self.content)
		self.assertIn("nugetconfig,${NUGET_CONFIG},${VCPKG_BINARY_CACHE_MODE}", self.content)
		self.assertIn('rm -f "${NUGET_CONFIG}"', self.content)

	def test_token_is_never_declared_as_arg_or_environment(self) -> None:
		self.assertIsNone(re.search(r"^(?:ARG|ENV)\s+(?:GITHUB_TOKEN|NUGET_AUTH_TOKEN)", self.content, re.MULTILINE))
		self.assertIn("--mount=type=secret,id=github_token", self.content)

	def test_runtime_contains_current_canary_contract(self) -> None:
		for expected in (
			"/srv/build/linux-release/bin/canary /bin/canary",
			"COPY LICENSE *.sql key.pem /canary/",
			"COPY data /canary/data",
			"COPY data-canary /canary/data-canary",
			"COPY data-otservbr-global /canary/data-otservbr-global",
			"COPY config.lua.dist /canary/config.lua",
			"ENTRYPOINT [\"/canary/start.sh\"]",
		):
			with self.subTest(expected=expected):
				self.assertIn(expected, self.content)

	def test_runtime_does_not_copy_otbm_explicitly(self) -> None:
		copy_lines = [line for line in self.content.splitlines() if line.lstrip().startswith("COPY ")]
		self.assertFalse(any(".otbm" in line for line in copy_lines))

	def test_compile_parallelism_is_bounded_and_configurable(self) -> None:
		self.assertIn("ARG BUILD_JOBS=2", self.content)
		self.assertIn('cmake --build --preset linux-release --parallel "${BUILD_JOBS}"', self.content)


class LocalRuntimeComposeContractTests(unittest.TestCase):
	def render_compose(
		self,
		*files: Path,
		environment_overrides: dict[str, str] | None = None,
	) -> dict:
		environment = os.environ.copy()
		environment["CANARY_LOCAL_PLATFORM"] = "linux/arm64"
		if environment_overrides:
			environment.update(environment_overrides)
		command = ["docker", "compose"]
		for compose_file in files:
			command.extend(("-f", str(compose_file)))
		command.extend(("config", "--format", "json"))
		completed = subprocess.run(
			command,
			cwd=REPO_ROOT,
			env=environment,
			check=True,
			capture_output=True,
			text=True,
		)
		return json.loads(completed.stdout)

	def test_runtime_overlay_declares_only_local_image_controls(self) -> None:
		content = LOCAL_COMPOSE.read_text(encoding="utf-8")
		self.assertIn("image: ats-server:local", content)
		self.assertIn("pull_policy: never", content)
		self.assertIn('platform: "${CANARY_LOCAL_PLATFORM}"', content)
		self.assertNotIn("build:", content)

	def test_merged_runtime_uses_local_image_without_server_build(self) -> None:
		config = self.render_compose(BASE_COMPOSE, LOCAL_COMPOSE)
		server = config["services"]["server"]
		self.assertEqual("ats-server:local", server["image"])
		self.assertEqual("never", server["pull_policy"])
		self.assertEqual("linux/arm64", server["platform"])
		self.assertNotIn("build", server)

	def test_merged_runtime_preserves_myaac_build(self) -> None:
		config = self.render_compose(BASE_COMPOSE, LOCAL_COMPOSE)
		self.assertIn("build", config["services"]["myaac"])

	def test_base_quickstart_keeps_published_image_default(self) -> None:
		content = BASE_COMPOSE.read_text(encoding="utf-8")
		self.assertIn('${CANARY_IMAGE:-ghcr.io/opentibiabr/canary:latest}', content)


class LocalBuildComposeContractTests(unittest.TestCase):
	def render_build_compose(self) -> dict:
		return LocalRuntimeComposeContractTests.render_compose(
			self,
			BASE_COMPOSE,
			LOCAL_COMPOSE,
			LOCAL_BUILD_COMPOSE,
		)

	def test_build_context_resolves_to_repository_root(self) -> None:
		server_build = self.render_build_compose()["services"]["server"]["build"]
		self.assertEqual(str(REPO_ROOT), server_build["context"])

	def test_build_uses_canonical_dockerfile(self) -> None:
		server_build = self.render_build_compose()["services"]["server"]["build"]
		self.assertEqual("docker/Dockerfile", server_build["dockerfile"])

	def test_build_overlay_scopes_definition_to_server_without_token_arg(self) -> None:
		content = LOCAL_BUILD_COMPOSE.read_text(encoding="utf-8")
		self.assertRegex(content, r"services:\s+server:\s+build:")
		self.assertNotIn("myaac:", content)
		self.assertNotIn("GITHUB_TOKEN", content)

	def test_build_output_keeps_local_image_tag(self) -> None:
		server = self.render_build_compose()["services"]["server"]
		self.assertEqual("ats-server:local", server["image"])


class LocalAuthComposeContractTests(unittest.TestCase):
	def render_auth_compose(self, token: str) -> dict:
		return LocalRuntimeComposeContractTests.render_compose(
			self,
			BASE_COMPOSE,
			LOCAL_COMPOSE,
			LOCAL_BUILD_COMPOSE,
			LOCAL_AUTH_COMPOSE,
			environment_overrides={"GITHUB_TOKEN": token},
		)

	def test_auth_secret_reads_from_github_token_environment(self) -> None:
		config = self.render_auth_compose("contract-sentinel-token")
		self.assertEqual("GITHUB_TOKEN", config["secrets"]["github_token"]["environment"])

	def test_auth_secret_is_granted_only_to_server_build(self) -> None:
		config = self.render_auth_compose("contract-sentinel-token")
		self.assertEqual(
			[{"source": "github_token", "target": "github_token"}],
			config["services"]["server"]["build"]["secrets"],
		)
		self.assertNotIn("secrets", config["services"]["myaac"]["build"])

	def test_no_token_compose_combination_has_no_build_secret(self) -> None:
		config = LocalRuntimeComposeContractTests.render_compose(
			self,
			BASE_COMPOSE,
			LOCAL_COMPOSE,
			LOCAL_BUILD_COMPOSE,
		)
		self.assertNotIn("secrets", config)
		self.assertNotIn("secrets", config["services"]["server"]["build"])

	def test_sentinel_token_is_absent_from_rendered_config_and_image_contract(self) -> None:
		sentinel = "contract-sentinel-token"
		rendered = json.dumps(self.render_auth_compose(sentinel), sort_keys=True)
		dockerfile = DOCKERFILE.read_text(encoding="utf-8")
		self.assertNotIn(sentinel, rendered)
		self.assertNotIn(sentinel, dockerfile)
		self.assertIn('rm -f "${NUGET_CONFIG}"', dockerfile)
		self.assertIsNone(re.search(r"^(?:ARG|ENV)\s+(?:GITHUB_TOKEN|NUGET_AUTH_TOKEN)", dockerfile, re.MULTILINE))


class ShellLauncherContractTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temp_dir = tempfile.TemporaryDirectory()
		self.temp_path = Path(self.temp_dir.name)
		self.fake_bin = self.temp_path / "bin"
		self.fake_bin.mkdir()
		self.log_path = self.temp_path / "docker.log"
		fake_docker = self.fake_bin / "docker"
		fake_docker.write_text(
			"""#!/usr/bin/env sh
printf '%s\\n' "$*" >> "$DOCKER_FAKE_LOG"
case "${1:-}" in
  info)
    [ "${FAKE_INFO_STATUS:-0}" -eq 0 ] || exit "$FAKE_INFO_STATUS"
    printf '%s\\n' "${FAKE_DOCKER_ARCH:-arm64}"
    ;;
  image)
    if [ "${2:-}" = inspect ]; then
      exit "${FAKE_IMAGE_STATUS:-0}"
    fi
    ;;
  compose)
    case " $* " in
      *" build server "*) exit "${FAKE_BUILD_STATUS:-0}" ;;
      *" up -d "*) exit "${FAKE_UP_STATUS:-0}" ;;
    esac
    ;;
esac
exit 0
""",
			encoding="utf-8",
		)
		fake_docker.chmod(0o755)

	def tearDown(self) -> None:
		self.temp_dir.cleanup()

	def run_launcher(self, *arguments: str, **environment_overrides: str) -> subprocess.CompletedProcess[str]:
		environment = os.environ.copy()
		environment["PATH"] = f"{self.fake_bin}{os.pathsep}{environment['PATH']}"
		environment["DOCKER_FAKE_LOG"] = str(self.log_path)
		environment["FAKE_DOCKER_ARCH"] = "arm64"
		environment["FAKE_IMAGE_STATUS"] = "0"
		for key in ("GITHUB_TOKEN", "VCPKG_FEED_URL", "VCPKG_FEED_USERNAME"):
			environment.pop(key, None)
		environment.update(environment_overrides)
		return subprocess.run(
			["sh", str(SHELL_LAUNCHER), *arguments],
			cwd=REPO_ROOT,
			env=environment,
			capture_output=True,
			text=True,
		)

	def docker_calls(self) -> list[str]:
		if not self.log_path.exists():
			return []
		return self.log_path.read_text(encoding="utf-8").splitlines()

	def test_no_argument_reuses_image_without_build_overlay(self) -> None:
		result = self.run_launcher()
		calls = self.docker_calls()
		self.assertEqual(0, result.returncode, result.stderr)
		self.assertTrue(any(call.startswith("image inspect ats-server:local") for call in calls))
		up_call = next(call for call in calls if " up -d " in f" {call} ")
		self.assertIn("docker-compose.local.yml", up_call)
		self.assertNotIn("docker-compose.local-build.yml", up_call)

	def test_rebuild_calls_server_build_before_runtime_up(self) -> None:
		result = self.run_launcher("--rebuild")
		calls = self.docker_calls()
		build_index = next(index for index, call in enumerate(calls) if " build server " in f" {call} ")
		up_index = next(index for index, call in enumerate(calls) if " up -d " in f" {call} ")
		self.assertEqual(0, result.returncode, result.stderr)
		self.assertLess(build_index, up_index)
		self.assertIn("docker-compose.local-build.yml", calls[build_index])
		self.assertNotIn("docker-compose.local-build.yml", calls[up_index])

	def test_missing_image_without_rebuild_reports_exact_correction(self) -> None:
		result = self.run_launcher(FAKE_IMAGE_STATUS="1")
		self.assertNotEqual(0, result.returncode)
		self.assertIn("sh docker/up-local.sh --rebuild", result.stderr)
		self.assertFalse(any(" up -d " in f" {call} " for call in self.docker_calls()))

	def test_build_failure_does_not_start_compose(self) -> None:
		result = self.run_launcher("--rebuild", FAKE_BUILD_STATUS="42")
		self.assertEqual(42, result.returncode)
		self.assertFalse(any(" up -d " in f" {call} " for call in self.docker_calls()))

	def test_amd64_engine_maps_to_linux_amd64(self) -> None:
		result = self.run_launcher(FAKE_DOCKER_ARCH="x86_64")
		self.assertEqual(0, result.returncode, result.stderr)
		self.assertIn("linux/amd64", result.stdout)

	def test_arm64_engine_maps_to_linux_arm64(self) -> None:
		result = self.run_launcher(FAKE_DOCKER_ARCH="aarch64")
		self.assertEqual(0, result.returncode, result.stderr)
		self.assertIn("linux/arm64", result.stdout)

	def test_unsupported_engine_architecture_fails_before_compose(self) -> None:
		result = self.run_launcher(FAKE_DOCKER_ARCH="s390x")
		self.assertNotEqual(0, result.returncode)
		self.assertIn("Unsupported Docker architecture: s390x", result.stderr)
		self.assertFalse(any(call.startswith("compose ") for call in self.docker_calls()))

	def test_token_mode_requires_feed_url(self) -> None:
		result = self.run_launcher(
			"--rebuild",
			GITHUB_TOKEN="super-secret-sentinel",
			VCPKG_FEED_USERNAME="ats-user",
		)
		self.assertNotEqual(0, result.returncode)
		self.assertIn("VCPKG_FEED_URL", result.stderr)

	def test_token_mode_requires_feed_username(self) -> None:
		result = self.run_launcher(
			"--rebuild",
			GITHUB_TOKEN="super-secret-sentinel",
			VCPKG_FEED_URL="https://example.invalid/nuget",
		)
		self.assertNotEqual(0, result.returncode)
		self.assertIn("VCPKG_FEED_USERNAME", result.stderr)

	def test_token_mode_adds_auth_overlay_only_to_build(self) -> None:
		result = self.run_launcher(
			"--rebuild",
			GITHUB_TOKEN="super-secret-sentinel",
			VCPKG_FEED_URL="https://example.invalid/nuget",
			VCPKG_FEED_USERNAME="ats-user",
		)
		calls = self.docker_calls()
		build_call = next(call for call in calls if " build server " in f" {call} ")
		up_call = next(call for call in calls if " up -d " in f" {call} ")
		self.assertEqual(0, result.returncode, result.stderr)
		self.assertIn("docker-compose.local-auth.yml", build_call)
		self.assertNotIn("docker-compose.local-auth.yml", up_call)
		self.assertNotIn("super-secret-sentinel", result.stdout + result.stderr + "\n".join(calls))

	def test_unknown_option_fails_before_docker_call_and_shows_usage(self) -> None:
		result = self.run_launcher("--unknown")
		self.assertNotEqual(0, result.returncode)
		self.assertIn("Usage:", result.stderr)
		self.assertEqual([], self.docker_calls())

	def test_logs_tag_platform_and_action_without_secrets(self) -> None:
		result = self.run_launcher()
		self.assertEqual(0, result.returncode, result.stderr)
		self.assertIn("ats-server:local", result.stdout)
		self.assertIn("linux/arm64", result.stdout)
		self.assertIn("reusing", result.stdout.lower())


if __name__ == "__main__":
	unittest.main(verbosity=2)
