# Publishes the :audioroute library to Maven Central (see audioroute/build.gradle).
#
# Credentials and the signing key are read from a private gradle.properties kept
# outside the repo (mavenCentralUsername, mavenCentralPassword, signing.keyId,
# signing.password, signing.secretKeyRingFile). They are passed to Gradle as
# ORG_GRADLE_PROJECT_* environment variables of this process only. A relative
# signing.secretKeyRingFile is resolved against the properties file's folder.
#
# Usage:
#   .\publish-maven.ps1 -Properties G:\path\to\private\maven\gradle.properties
#       uploads and releases automatically once Central Portal validation passes.
#       A released version is permanent: it can never be changed or deleted.
#   .\publish-maven.ps1 -Properties ... -Task :audioroute:publishToMavenCentral
#       uploads only; release it by pressing "Publish" under Deployments on
#       central.sonatype.com (or "Drop" to discard it)
#   .\publish-maven.ps1 -Properties ... -Task :audioroute:publishToMavenLocal   # dry run
#
# Requires JAVA_HOME to point to JDK 17 or 21 (Gradle 9 does not run on newer JDKs).

param(
    [string]$Properties = (Join-Path $PSScriptRoot '..\private\maven\gradle.properties'),
    [string]$Task = ':audioroute:publishAndReleaseToMavenCentral'
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Properties)) { throw "Properties file not found: $Properties" }
$propsDir = Split-Path -Parent (Resolve-Path $Properties).Path

foreach ($line in Get-Content $Properties) {
    $l = $line.Trim()
    if ($l -eq '' -or $l.StartsWith('#') -or $l.StartsWith('!')) { continue }
    $i = $l.IndexOf('=')
    if ($i -lt 1) { continue }
    $key = $l.Substring(0, $i).Trim()
    $value = $l.Substring($i + 1).Trim()
    if ($key -eq 'signing.secretKeyRingFile' -and -not [IO.Path]::IsPathRooted($value)) {
        $value = Join-Path $propsDir $value
    }
    [Environment]::SetEnvironmentVariable("ORG_GRADLE_PROJECT_$key", $value, 'Process')
}

# Gradle/javac write warnings to stderr: don't let PowerShell 5.1 treat them as fatal
$ErrorActionPreference = 'Continue'
& (Join-Path $PSScriptRoot 'gradlew.bat') clean $Task
exit $LASTEXITCODE
