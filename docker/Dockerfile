# syntax=docker/dockerfile:1.7

FROM ubuntu:24.04 AS dependencies

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH
ARG VCPKG_FEED_URL
ARG VCPKG_FEED_USERNAME
ARG VCPKG_BINARY_CACHE_MODE=read
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV TZ=Etc/UTC

RUN case "${TARGETARCH}" in \
	amd64|arm64) ;; \
	*) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
	esac

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
	--mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
	apt-get update && apt-get install -y --no-install-recommends \
	autoconf automake build-essential ca-certificates cmake curl git libtool \
	libluajit-5.1-common libluajit-5.1-dev mono-complete ninja-build \
	pkg-config python3 tar tzdata unzip wget zip \
	&& ln -snf "/usr/share/zoneinfo/${TZ}" /etc/localtime \
	&& echo "${TZ}" > /etc/timezone \
	&& apt-get clean \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /opt
COPY vcpkg.json /opt/
RUN vcpkg_commit_id=$(grep '.builtin-baseline' vcpkg.json | awk -F: '{print $2}' | tr -d '," ') \
	&& echo "vcpkg commit ID: ${vcpkg_commit_id}" \
	&& git clone https://github.com/Microsoft/vcpkg.git \
	&& cd vcpkg \
	&& git checkout "${vcpkg_commit_id}" \
	&& ./bootstrap-vcpkg.sh

WORKDIR /opt/vcpkg_manifest
COPY vcpkg.json /opt/vcpkg_manifest/

RUN --mount=type=secret,id=github_token \
	--mount=type=cache,target=/opt/vcpkg/downloads \
	--mount=type=cache,target=/opt/vcpkg/buildtrees \
	--mount=type=cache,target=/opt/vcpkg/packages \
	--mount=type=cache,target=/root/.cache/vcpkg \
	/bin/bash -euo pipefail -c '\
		case "${TARGETARCH}" in \
			amd64) VCPKG_TRIPLET=x64-linux ;; \
			arm64) VCPKG_TRIPLET=arm64-linux; export VCPKG_FORCE_SYSTEM_BINARIES=1 ;; \
			*) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
		esac; \
		if [ -s /run/secrets/github_token ]; then \
			if [ -z "${VCPKG_FEED_URL:-}" ]; then \
				echo "Missing VCPKG_FEED_URL build arg" >&2; \
				exit 1; \
			fi; \
			if [ -z "${VCPKG_FEED_USERNAME:-}" ]; then \
				echo "Missing VCPKG_FEED_USERNAME build arg" >&2; \
				exit 1; \
			fi; \
			case "${VCPKG_BINARY_CACHE_MODE}" in \
				read|write|readwrite) ;; \
				*) echo "Invalid VCPKG_BINARY_CACHE_MODE: ${VCPKG_BINARY_CACHE_MODE}" >&2; exit 1 ;; \
			esac; \
			NUGET_AUTH_TOKEN=$(cat /run/secrets/github_token); \
			NUGET_CONFIG=/tmp/nuget.config; \
			printf "%s\n" \
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>" \
				"<configuration>" \
				"  <packageSources>" \
				"    <add key=\"GitHubPackages\" value=\"${VCPKG_FEED_URL}\" />" \
				"  </packageSources>" \
				"  <packageSourceCredentials>" \
				"    <GitHubPackages>" \
				"      <add key=\"Username\" value=\"${VCPKG_FEED_USERNAME}\" />" \
				"      <add key=\"ClearTextPassword\" value=\"${NUGET_AUTH_TOKEN}\" />" \
				"    </GitHubPackages>" \
				"  </packageSourceCredentials>" \
				"  <config>" \
				"    <add key=\"defaultPushSource\" value=\"GitHubPackages\" />" \
				"  </config>" \
				"</configuration>" \
				> "${NUGET_CONFIG}"; \
			NUGET_EXE=$(/opt/vcpkg/vcpkg fetch nuget | tail -n 1 | tr -d "\r"); \
			if [ ! -f "${NUGET_EXE}" ]; then \
				echo "nuget.exe not found: ${NUGET_EXE}" >&2; \
				exit 1; \
			fi; \
			mono "${NUGET_EXE}" setapikey "${NUGET_AUTH_TOKEN}" -Source "${VCPKG_FEED_URL}" -ConfigFile "${NUGET_CONFIG}"; \
			export VCPKG_NUGET_API_KEY="${NUGET_AUTH_TOKEN}"; \
			export VCPKG_BINARY_SOURCES="clear;nugetconfig,${NUGET_CONFIG},${VCPKG_BINARY_CACHE_MODE};nugettimeout,1200"; \
			echo "Using authenticated vcpkg binary cache mode: ${VCPKG_BINARY_CACHE_MODE}"; \
		else \
			export VCPKG_BINARY_SOURCES="clear;files,/root/.cache/vcpkg/archives,readwrite"; \
			echo "Using local vcpkg binary cache"; \
		fi; \
		/opt/vcpkg/vcpkg install \
			--x-manifest-root=/opt/vcpkg_manifest \
			--x-install-root=/opt/vcpkg_installed \
			--triplet="${VCPKG_TRIPLET}" \
			--host-triplet="${VCPKG_TRIPLET}"; \
		if [ -n "${NUGET_CONFIG:-}" ]; then rm -f "${NUGET_CONFIG}"; fi; \
		unset NUGET_AUTH_TOKEN VCPKG_NUGET_API_KEY'

FROM dependencies AS build

ARG TARGETARCH
ARG BUILD_JOBS=2
COPY . /srv/
WORKDIR /srv
COPY --from=dependencies /opt/vcpkg_installed /srv/vcpkg_installed

RUN /bin/bash -euo pipefail -c '\
		case "${TARGETARCH}" in \
			amd64) VCPKG_TRIPLET=x64-linux ;; \
			arm64) VCPKG_TRIPLET=arm64-linux; export VCPKG_FORCE_SYSTEM_BINARIES=1 ;; \
			*) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
		esac; \
		export VCPKG_ROOT=/opt/vcpkg; \
		cmake --preset linux-release \
			-DTOGGLE_BIN_FOLDER=ON \
			-DVCPKG_MANIFEST_INSTALL=OFF \
			-DVCPKG_INSTALLED_DIR=/srv/vcpkg_installed \
			-DVCPKG_TARGET_TRIPLET="${VCPKG_TRIPLET}" \
			-DVCPKG_HOST_TRIPLET="${VCPKG_TRIPLET}"; \
		cmake --build --preset linux-release --parallel "${BUILD_JOBS}"'

FROM dependencies AS test

ARG TARGETARCH
ARG BUILD_JOBS=2
COPY . /srv/
WORKDIR /srv
COPY --from=dependencies /opt/vcpkg_installed /srv/vcpkg_installed

RUN /bin/bash -euo pipefail -c '\
		case "${TARGETARCH}" in \
			amd64) VCPKG_TRIPLET=x64-linux ;; \
			arm64) VCPKG_TRIPLET=arm64-linux; export VCPKG_FORCE_SYSTEM_BINARIES=1 ;; \
			*) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
		esac; \
		export VCPKG_ROOT=/opt/vcpkg; \
		/opt/vcpkg/vcpkg install \
			--x-manifest-root=/srv \
			--x-install-root=/srv/vcpkg_installed \
			--triplet="${VCPKG_TRIPLET}" \
			--host-triplet="${VCPKG_TRIPLET}" \
			--x-feature=tests; \
		cmake --preset linux-debug \
			-DTOGGLE_BIN_FOLDER=ON \
			-DVCPKG_MANIFEST_INSTALL=OFF \
			-DVCPKG_INSTALLED_DIR=/srv/vcpkg_installed \
			-DVCPKG_TARGET_TRIPLET="${VCPKG_TRIPLET}" \
			-DVCPKG_HOST_TRIPLET="${VCPKG_TRIPLET}"; \
		cmake --build --preset linux-debug --target canary_ut --parallel "${BUILD_JOBS}"'

WORKDIR /srv/build/linux-debug/tests/unit
ENTRYPOINT ["./canary_ut"]

FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV TZ=Etc/UTC
VOLUME ["/data"]

COPY --from=build /srv/build/linux-release/bin/canary /bin/canary
COPY LICENSE *.sql key.pem /canary/
COPY data /canary/data
COPY data-canary /canary/data-canary
COPY data-otservbr-global /canary/data-otservbr-global
COPY config.lua.dist /canary/config.lua

WORKDIR /canary

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
	--mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
	apt-get update && apt-get install -y --no-install-recommends \
	ca-certificates curl libluajit-5.1-common libluajit-5.1-dev mariadb-client tzdata wget \
	&& ln -snf "/usr/share/zoneinfo/${TZ}" /etc/localtime \
	&& echo "${TZ}" > /etc/timezone \
	&& apt-get clean \
	&& rm -rf /var/lib/apt/lists/*

COPY docker/data/01-test_account.sql 01-test_account.sql
COPY docker/data/02-test_account_players.sql 02-test_account_players.sql
COPY --chmod=755 docker/data/start.sh start.sh

ENTRYPOINT ["/canary/start.sh"]
