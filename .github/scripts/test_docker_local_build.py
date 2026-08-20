#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DOCKERFILE = REPO_ROOT / "docker" / "Dockerfile"


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


if __name__ == "__main__":
	unittest.main(verbosity=2)
