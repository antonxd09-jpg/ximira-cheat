#!/usr/bin/env sh
set -e

GRADLE_VERSION=8.7
DIST_URL="https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip"
WRAPPER_DIR="gradle/wrapper"
JAR="$WRAPPER_DIR/gradle-wrapper.jar"

if [ ! -f "$JAR" ]; then
  echo "Downloading Gradle wrapper jar..."
  mkdir -p "$WRAPPER_DIR"
  TMP=$(mktemp -d)
  curl -fsSL "$DIST_URL" -o "$TMP/gradle.zip"
  unzip -q "$TMP/gradle.zip" -d "$TMP"
  cp "$TMP/gradle-${GRADLE_VERSION}/lib/plugins/gradle-wrapper-${GRADLE_VERSION}.jar" "$JAR" 2>/dev/null || \
  cp "$TMP/gradle-${GRADLE_VERSION}/lib/gradle-wrapper-${GRADLE_VERSION}.jar" "$JAR"
  rm -rf "$TMP"
fi

exec java -classpath "$JAR" org.gradle.wrapper.GradleWrapperMain "$@"
