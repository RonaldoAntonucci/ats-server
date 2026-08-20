#!/usr/bin/env python3

import json
import os
import re
import subprocess
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DOCKERFILE = REPO_ROOT / "docker" / "Dockerfile"
BASE_COMPOSE = REPO_ROOT / "docker" / "docker-compose.yml"
LOCAL_COMPOSE = REPO_ROOT / "docker" / "docker-compose.local.yml"
LOCAL_BUILD_COMPOSE = REPO_ROOT / "docker" / "docker-compose.local-build.yml"


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
	def render_compose(self, *files: Path) -> dict:
		environment = os.environ.copy()
		environment["CANARY_LOCAL_PLATFORM"] = "linux/arm64"
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


if __name__ == "__main__":
	unittest.main(verbosity=2)
