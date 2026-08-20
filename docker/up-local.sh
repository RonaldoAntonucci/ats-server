#!/usr/bin/env sh
set -eu

usage() {
	printf '%s\n' "Usage: sh docker/up-local.sh [--rebuild]" >&2
	exit 2
}

fail() {
	printf '%s\n' "$1" >&2
	exit 1
}

rebuild=false
case "$#" in
	0) ;;
	1)
		[ "$1" = "--rebuild" ] || usage
		rebuild=true
		;;
	*) usage ;;
esac

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base_compose="${script_dir}/docker-compose.yml"
runtime_compose="${script_dir}/docker-compose.local.yml"
build_compose="${script_dir}/docker-compose.local-build.yml"
auth_compose="${script_dir}/docker-compose.local-auth.yml"
image_tag="ats-server:local"

command -v docker >/dev/null 2>&1 || fail "Docker was not found. Install Docker, start it, then run this script again: https://docs.docker.com/get-started/get-docker/"

if ! docker_architecture=$(docker info --format '{{.Architecture}}' 2>/dev/null); then
	fail "Docker is installed but the daemon is not running. Start Docker Desktop or the Docker service, then run this script again."
fi

case "$docker_architecture" in
	amd64|x86_64) local_platform="linux/amd64" ;;
	arm64|aarch64) local_platform="linux/arm64" ;;
	*) fail "Unsupported Docker architecture: ${docker_architecture}. Supported architectures: amd64 and arm64." ;;
esac
export CANARY_LOCAL_PLATFORM="$local_platform"

docker compose version >/dev/null 2>&1 || fail "Docker Compose v2 was not found. Update Docker Desktop or install the Docker Compose plugin."

compose_runtime() {
	docker compose \
		-f "$base_compose" \
		-f "$runtime_compose" \
		"$@"
}

compose_build() {
	if [ -n "${GITHUB_TOKEN:-}" ]; then
		docker compose \
			-f "$base_compose" \
			-f "$runtime_compose" \
			-f "$build_compose" \
			-f "$auth_compose" \
			"$@"
	else
		docker compose \
			-f "$base_compose" \
			-f "$runtime_compose" \
			-f "$build_compose" \
			"$@"
	fi
}

printf '%s\n' "Local Canary image: ${image_tag}"
printf '%s\n' "Platform: ${local_platform}"

if [ "$rebuild" = true ]; then
	if [ -n "${GITHUB_TOKEN:-}" ]; then
		[ -n "${VCPKG_FEED_URL:-}" ] || fail "GITHUB_TOKEN requires VCPKG_FEED_URL for the authenticated vcpkg cache."
		[ -n "${VCPKG_FEED_USERNAME:-}" ] || fail "GITHUB_TOKEN requires VCPKG_FEED_USERNAME for the authenticated vcpkg cache."
	fi

	printf '%s\n' "Action: building ${image_tag}"
	compose_build build server
	docker image inspect "$image_tag" >/dev/null 2>&1 || fail "The build completed without creating ${image_tag}; the Compose stack was not started."
else
	printf '%s\n' "Action: reusing ${image_tag}"
	docker image inspect "$image_tag" >/dev/null 2>&1 || fail "Local image ${image_tag} was not found. Run: sh docker/up-local.sh --rebuild"
fi

compose_runtime up -d --remove-orphans
