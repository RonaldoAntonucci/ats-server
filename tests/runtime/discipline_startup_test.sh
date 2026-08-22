#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
compose_file="${repo_root}/docker/docker-compose.yml"
project_name="canary-discipline-startup-${RANDOM}${RANDOM}"
runtime_image="ats-server:disciplines-runtime"
temp_dir="$(mktemp -d)"

cleanup() {
	docker compose --project-name "${project_name}" --env-file "${temp_dir}/.env" -f "${compose_file}" down -v --remove-orphans >/dev/null 2>&1 || true
	rm -rf "${temp_dir}"
}
trap cleanup EXIT

printf '%s\n' \
	'CANARY_DB_HOST=db' \
	'CANARY_DB_PORT=3306' \
	'CANARY_DB_NAME=canary' \
	'CANARY_DB_USER=canary' \
	'CANARY_DB_PASSWORD=canary' \
	'CANARY_DB_ROOT_PASSWORD=root' \
	'CANARY_SERVER_NAME=Discipline startup test' \
	'CANARY_SERVER_IP=127.0.0.1' \
	'CANARY_LOGIN_PORT=27171' \
	'CANARY_GAME_PORT=27172' \
	'CANARY_LEGACY_1100_GAME_PORT=27174' \
	'CANARY_LEGACY_860_GAME_PORT=27175' \
	'CANARY_STATUS_PORT=27173' \
	'CANARY_STATUS_TIMEOUT=5000' \
	'CANARY_TEST_ACCOUNTS=false' \
	'CANARY_DATA_PACK=data-canary' \
	'CANARY_MAP_URL=' \
	'CANARY_IMAGE=ats-server:disciplines-runtime' >"${temp_dir}/.env"

compose=(docker compose --project-name "${project_name}" --env-file "${temp_dir}/.env" -f "${compose_file}")

wait_for_database() {
	for _ in $(seq 1 60); do
		if "${compose[@]}" exec -T db mariadb-admin ping -h localhost -u root -proot --silent >/dev/null 2>&1; then
			return 0
		fi
		sleep 2
	done

	echo "Database did not become ready." >&2
	return 1
}

wait_for_server_online() {
	local container_id="$1"

	for _ in $(seq 1 90); do
		if docker logs "${container_id}" 2>&1 | grep -Fq "Discipline startup test server online!"; then
			return 0
		fi
		sleep 2
	done

	echo "Server did not become online with a valid discipline catalog." >&2
	docker logs "${container_id}" >&2 || true
	return 1
}

require_invalid_log() {
	local pattern="$1"

	if ! grep -Fq "${pattern}" "${temp_dir}/invalid.log"; then
		echo "Invalid catalog log did not contain: ${pattern}" >&2
		sed -n '1,200p' "${temp_dir}/invalid.log" >&2
		exit 1
	fi
}

cd "${repo_root}"
if ! docker image inspect "${runtime_image}" >/dev/null 2>&1; then
	docker build --quiet --file docker/Dockerfile --target runtime --tag "${runtime_image}" --build-arg BUILD_JOBS="${BUILD_JOBS:-2}" .
else
	echo "Reusing existing runtime image: ${runtime_image}"
fi

"${compose[@]}" up -d db
wait_for_database

valid_container="${project_name}-valid"
"${compose[@]}" run --no-deps --name "${valid_container}" -d -T server
wait_for_server_online "${valid_container}"

docker logs "${valid_container}" >"${temp_dir}/valid.log" 2>&1
if grep -Fq "Cannot load: XML/disciplines.xml" "${temp_dir}/valid.log"; then
	echo "Valid discipline catalog was rejected." >&2
	sed -n '1,200p' "${temp_dir}/valid.log" >&2
	exit 1
fi

docker stop "${valid_container}" >/dev/null
docker rm "${valid_container}" >/dev/null

printf '%s\n' \
	'<?xml version="1.0" encoding="UTF-8"?>' \
	'<disciplines>' \
	'  <discipline id="1" name="Armamento">' \
	'    <attribute id="for" perLevel="not-a-number" />' \
	'  </discipline>' \
	'</disciplines>' >"${temp_dir}/disciplines.xml"

set +e
"${compose[@]}" run --no-deps --rm -T \
	-v "${temp_dir}/disciplines.xml:/canary/data/XML/disciplines.xml:ro" \
	server >"${temp_dir}/invalid.log" 2>&1
invalid_status=$?
set -e

if [[ "${invalid_status}" -eq 0 ]]; then
	echo "Invalid discipline catalog unexpectedly allowed startup." >&2
	sed -n '1,200p' "${temp_dir}/invalid.log" >&2
	exit 1
fi

require_invalid_log "[DisciplineCatalog]"
require_invalid_log "path=data/XML/disciplines.xml"
require_invalid_log "discipline=1"
require_invalid_log "field=attribute.perLevel"
require_invalid_log "reason=must be a non-negative uint32"
require_invalid_log "Cannot load: XML/disciplines.xml"

if grep -Fq "Loading core scripts" "${temp_dir}/invalid.log"; then
	echo "Core Lua scripts were reached after an invalid discipline catalog." >&2
	sed -n '1,200p' "${temp_dir}/invalid.log" >&2
	exit 1
fi

echo "Discipline startup checks passed."
