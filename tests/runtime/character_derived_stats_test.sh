#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
compose_file="${repo_root}/docker/docker-compose.yml"
project_name="canary-derived-stats-${RANDOM}${RANDOM}"
runtime_image="ats-server:local"
test_image="ats-server:derived-stats-test"
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
	'CANARY_SERVER_NAME=Derived stats runtime test' \
	'CANARY_SERVER_IP=127.0.0.1' \
	'CANARY_LOGIN_PORT=27181' \
	'CANARY_GAME_PORT=27182' \
	'CANARY_LEGACY_1100_GAME_PORT=27184' \
	'CANARY_LEGACY_860_GAME_PORT=27185' \
	'CANARY_STATUS_PORT=27183' \
	'CANARY_STATUS_TIMEOUT=5000' \
	'CANARY_TEST_ACCOUNTS=false' \
	'CANARY_DATA_PACK=data-canary' \
	'CANARY_MAP_URL=' \
	"CANARY_IMAGE=${runtime_image}" >"${temp_dir}/.env"

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
	local container_name="$1"

	for _ in $(seq 1 90); do
		if docker logs "${container_name}" 2>&1 | grep -Fq "Derived stats runtime test server online!"; then
			return 0
		fi
		if [[ "$(docker inspect --format '{{.State.Running}}' "${container_name}" 2>/dev/null || true)" != "true" ]]; then
			break
		fi
		sleep 2
	done

	echo "Server did not become online with a valid derived-stats configuration." >&2
	docker logs "${container_name}" >&2 || true
	return 1
}

require_log() {
	local file="$1"
	local pattern="$2"

	if ! grep -Fq "${pattern}" "${file}"; then
		echo "Expected log was not found: ${pattern}" >&2
		sed -n '1,220p' "${file}" >&2
		exit 1
	fi
}

run_server() {
	local container_name="$1"
	local config_source="$2"

	"${compose[@]}" run --no-deps --name "${container_name}" -d -T \
		--entrypoint /bin/bash \
		-v "${config_source}:/canary/config.source.lua:ro" \
		server -ec 'cp /canary/config.source.lua /canary/config.lua; exec /canary/start.sh'
}

cd "${repo_root}"

# These focused tests observe the immutable snapshot directly. The process-level
# checks below prove the same accepted/rejected candidates through real startup
# and SIGHUP paths, without rebuilding either image.
docker run --rm "${test_image}" --gtest_filter='ConfigManagerDerivedStatsTest.LoadsNineIndependentCustomValues:ConfigManagerDerivedStatsTest.AcceptsZeroForOneRelationship:ConfigManagerDerivedStatsTest.InvalidReloadPreservesGenericConfigurationAndSnapshot'

cp config.lua.dist "${temp_dir}/defaults.lua"
cp config.lua.dist "${temp_dir}/custom.lua"
printf '%s\n' \
	'characterPotToPhysicalAttackMultiplier = 0' \
	'characterPotToPhysicalDefenseMultiplier = 2' \
	'characterTecToPrecisionMultiplier = 3' \
	'characterVigToMaximumHealthMultiplier = 4' \
	'characterVigToPhysicalDefenseMultiplier = 5' \
	'characterSinToMagicalAttackMultiplier = 6' \
	'characterSinToMagicalDefenseMultiplier = 7' \
	'characterEspToMaximumManaMultiplier = 8' \
	'characterEspToMagicalDefenseMultiplier = 9' >>"${temp_dir}/custom.lua"

cp "${temp_dir}/custom.lua" "${temp_dir}/invalid-startup.lua"
printf '%s\n' 'characterTecToPrecisionMultiplier = -1' >>"${temp_dir}/invalid-startup.lua"

cp "${temp_dir}/custom.lua" "${temp_dir}/invalid-reload.lua"
printf '%s\n' 'characterEspToMaximumManaMultiplier = -1' >>"${temp_dir}/invalid-reload.lua"

"${compose[@]}" up -d db
wait_for_database

default_container="${project_name}-defaults"
run_server "${default_container}" "${temp_dir}/defaults.lua" >/dev/null
wait_for_server_online "${default_container}"
docker logs "${default_container}" >"${temp_dir}/defaults.log" 2>&1
if grep -Fq "[ConfigManager::loadDerivedStatMultipliers]" "${temp_dir}/defaults.log"; then
	echo "Documented default derived-stats configuration emitted an unexpected diagnostic." >&2
	sed -n '1,220p' "${temp_dir}/defaults.log" >&2
	exit 1
fi
docker stop "${default_container}" >/dev/null
docker rm "${default_container}" >/dev/null

custom_container="${project_name}-custom"
run_server "${custom_container}" "${temp_dir}/custom.lua" >/dev/null
wait_for_server_online "${custom_container}"
for assignment in \
	'characterPotToPhysicalAttackMultiplier = 0' \
	'characterPotToPhysicalDefenseMultiplier = 2' \
	'characterTecToPrecisionMultiplier = 3' \
	'characterVigToMaximumHealthMultiplier = 4' \
	'characterVigToPhysicalDefenseMultiplier = 5' \
	'characterSinToMagicalAttackMultiplier = 6' \
	'characterSinToMagicalDefenseMultiplier = 7' \
	'characterEspToMaximumManaMultiplier = 8' \
	'characterEspToMagicalDefenseMultiplier = 9'; do
	if ! docker exec "${custom_container}" grep -Fxq "${assignment}" /canary/config.lua; then
		echo "Custom runtime configuration was not installed completely: ${assignment}" >&2
		exit 1
	fi
done

before_lines="$(docker logs "${custom_container}" 2>&1 | wc -l | tr -d ' ')"
docker cp "${temp_dir}/invalid-reload.lua" "${custom_container}:/canary/config.lua" >/dev/null
docker kill --signal HUP "${custom_container}" >/dev/null

for _ in $(seq 1 30); do
	docker logs "${custom_container}" >"${temp_dir}/reload-full.log" 2>&1
	tail -n "+$((before_lines + 1))" "${temp_dir}/reload-full.log" >"${temp_dir}/reload.log"
	if grep -Fq "Failed to reload config" "${temp_dir}/reload.log"; then
		break
	fi
	sleep 1
done

require_log "${temp_dir}/reload.log" "SIGHUP received, reloading config files..."
require_log "${temp_dir}/reload.log" "key=characterEspToMaximumManaMultiplier reason=negative value"
require_log "${temp_dir}/reload.log" "Failed to reload config"
if grep -Fq "Reloaded config" "${temp_dir}/reload.log"; then
	echo "Invalid SIGHUP reload reported a false configuration success." >&2
	sed -n '1,220p' "${temp_dir}/reload.log" >&2
	exit 1
fi
if [[ "$(docker inspect --format '{{.State.Running}}' "${custom_container}")" != "true" ]]; then
	echo "Invalid SIGHUP reload terminated the server instead of preserving the prior snapshot." >&2
	exit 1
fi
docker stop "${custom_container}" >/dev/null
docker rm "${custom_container}" >/dev/null

set +e
"${compose[@]}" run --no-deps --rm -T \
	--entrypoint /bin/bash \
	-v "${temp_dir}/invalid-startup.lua:/canary/config.source.lua:ro" \
	server -ec 'cp /canary/config.source.lua /canary/config.lua; exec /canary/start.sh' >"${temp_dir}/invalid-startup.log" 2>&1
invalid_status=$?
set -e

if [[ "${invalid_status}" -eq 0 ]]; then
	echo "Invalid derived-stats configuration unexpectedly allowed startup." >&2
	sed -n '1,220p' "${temp_dir}/invalid-startup.log" >&2
	exit 1
fi
require_log "${temp_dir}/invalid-startup.log" "key=characterTecToPrecisionMultiplier reason=negative value"
require_log "${temp_dir}/invalid-startup.log" "Cannot load: config.lua"
if grep -Fq "server online!" "${temp_dir}/invalid-startup.log"; then
	echo "Invalid derived-stats configuration reached the online state." >&2
	exit 1
fi

echo "Character derived stats runtime checks passed."
