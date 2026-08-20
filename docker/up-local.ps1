[CmdletBinding()]
param(
	[switch]$Rebuild
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
	$PSNativeCommandUseErrorActionPreference = $false
}

function Stop-WithMessage {
	param([string]$Message)

	[Console]::Error.WriteLine($Message)
	exit 1
}

function Invoke-DockerQuiet {
	param([string[]]$Arguments)

	& docker @Arguments > $null 2> $null
	return $LASTEXITCODE
}

if ($null -eq (Get-Command docker -ErrorAction SilentlyContinue)) {
	Stop-WithMessage "Docker was not found. Install Docker Desktop, start it, then run this script again: https://docs.docker.com/get-started/get-docker/"
}

$dockerArchitectureOutput = & docker info --format "{{.Architecture}}" 2> $null
if ($LASTEXITCODE -ne 0) {
	Stop-WithMessage "Docker is installed but the daemon is not running. Start Docker Desktop or the Docker service, then run this script again."
}
$dockerArchitecture = ([string]($dockerArchitectureOutput | Select-Object -First 1)).Trim()

switch ($dockerArchitecture) {
	"amd64" { $localPlatform = "linux/amd64" }
	"x86_64" { $localPlatform = "linux/amd64" }
	"arm64" { $localPlatform = "linux/arm64" }
	"aarch64" { $localPlatform = "linux/arm64" }
	default { Stop-WithMessage "Unsupported Docker architecture: $dockerArchitecture. Supported architectures: amd64 and arm64." }
}
$env:CANARY_LOCAL_PLATFORM = $localPlatform

if ((Invoke-DockerQuiet -Arguments @("compose", "version")) -ne 0) {
	Stop-WithMessage "Docker Compose v2 was not found. Update Docker Desktop or install the Docker Compose plugin."
}

$baseCompose = Join-Path $PSScriptRoot "docker-compose.yml"
$runtimeCompose = Join-Path $PSScriptRoot "docker-compose.local.yml"
$buildCompose = Join-Path $PSScriptRoot "docker-compose.local-build.yml"
$authCompose = Join-Path $PSScriptRoot "docker-compose.local-auth.yml"
$imageTag = "ats-server:local"

Write-Host "Local Canary image: $imageTag"
Write-Host "Platform: $localPlatform"

if ($Rebuild) {
	$buildArguments = @(
		"compose",
		"-f", $baseCompose,
		"-f", $runtimeCompose,
		"-f", $buildCompose
	)

	if ($env:GITHUB_TOKEN) {
		if (-not $env:VCPKG_FEED_URL) {
			Stop-WithMessage "GITHUB_TOKEN requires VCPKG_FEED_URL for the authenticated vcpkg cache."
		}
		if (-not $env:VCPKG_FEED_USERNAME) {
			Stop-WithMessage "GITHUB_TOKEN requires VCPKG_FEED_USERNAME for the authenticated vcpkg cache."
		}
		$buildArguments += @("-f", $authCompose)
	}

	Write-Host "Action: building $imageTag"
	$buildArguments += @("build", "server")
	& docker @buildArguments
	if ($LASTEXITCODE -ne 0) {
		exit $LASTEXITCODE
	}
	if ((Invoke-DockerQuiet -Arguments @("image", "inspect", $imageTag)) -ne 0) {
		Stop-WithMessage "The build completed without creating $imageTag; the Compose stack was not started."
	}
} else {
	Write-Host "Action: reusing $imageTag"
	if ((Invoke-DockerQuiet -Arguments @("image", "inspect", $imageTag)) -ne 0) {
		Stop-WithMessage "Local image $imageTag was not found. Run: .\docker\up-local.ps1 -Rebuild"
	}
}

$upArguments = @(
	"compose",
	"-f", $baseCompose,
	"-f", $runtimeCompose,
	"up", "-d", "--remove-orphans"
)
& docker @upArguments
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}
