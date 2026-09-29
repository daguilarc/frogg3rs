#!/usr/bin/env bash
# Re-signs a release APK with a real signing key, out of band from the
# Gradle build. Projucer applies one signing config (the debug keystore) to
# both build types (design.md, "Release"), so the generated project never
# sees the release key; this script strips that debug signature, zipaligns,
# and signs with apksigner from a keystore this script is never given more
# of than a path, an alias and a password -- never a keystore this repository
# ships.
#
# Usage: app/android/sign-apk.sh <input.apk> <output.apk>
#
# Environment (all required):
#   KEYSTORE           path to the signing keystore (a JKS or PKCS12 file)
#   KEY_ALIAS          alias of the key inside it
#   KEYSTORE_PASSWORD  password for both the keystore and the key (the
#                      shape 6.5's CI secret and 4.5's generated-for-the-run
#                      keystore both use: one password, not two)
#
# Verification: after signing, `apksigner verify --print-certs` must show
# exactly one signer, and that signer's SHA-256 must equal the keystore
# certificate's own SHA-256 (from `keytool -list -v`). Any other outcome
# (zero signers, more than one, or a mismatched fingerprint) exits non-zero
# -- this is the check a maliciously or accidentally multi-signed, or
# wrong-key-signed, APK must fail.
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <input.apk> <output.apk>" >&2
  exit 1
fi

INPUT_APK="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUTPUT_APK="$1_OUTPUT_PLACEHOLDER"
OUTPUT_APK="$2"

: "${KEYSTORE:?KEYSTORE (path to the signing keystore) must be set}"
: "${KEY_ALIAS:?KEY_ALIAS (the alias of the key inside the keystore) must be set}"
: "${KEYSTORE_PASSWORD:?KEYSTORE_PASSWORD (used for both the keystore and the key) must be set}"

if [ ! -f "$INPUT_APK" ]; then
  echo "sign-apk.sh: no such APK: $INPUT_APK" >&2
  exit 1
fi
if [ ! -f "$KEYSTORE" ]; then
  echo "sign-apk.sh: no such keystore: $KEYSTORE" >&2
  exit 1
fi

# The newest build-tools directory under the Android SDK, the same
# portable lookup app/android/build-android.sh uses for its own AAPT2
# default (see that script's comment): works on this Mac's SDK layout and
# on a CI runner's own, without one build-tools version's path baked in.
ANDROID_SDK_DIR="${ANDROID_SDK_DIR:-${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}}"
BUILD_TOOLS="${BUILD_TOOLS:-$(find "$ANDROID_SDK_DIR/build-tools" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -1)}"
ZIPALIGN="${ZIPALIGN:-$BUILD_TOOLS/zipalign}"
APKSIGNER="${APKSIGNER:-$BUILD_TOOLS/apksigner}"
KEYTOOL="${KEYTOOL:-keytool}"

# apksigner is a JVM tool (a shell launcher over a jar); without JAVA_HOME
# set it falls back to the system `java` stub, which on this Mac has no JDK
# registered and just prints "Unable to locate a Java Runtime." Same default
# as app/android/build-android.sh: Android Studio's own bundled JBR, unless
# the caller (a CI job with its own Temurin JAVA_HOME) already set one.
export JAVA_HOME="${JAVA_HOME:-/Applications/Android Studio.app/Contents/jbr/Contents/Home}"

for tool_var in ZIPALIGN APKSIGNER; do
  tool_path="${!tool_var}"
  if [ ! -x "$tool_path" ]; then
    echo "sign-apk.sh: no executable $tool_var at $tool_path" >&2
    exit 1
  fi
done

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

UNSIGNED_APK="$WORK_DIR/unsigned.apk"
ALIGNED_APK="$WORK_DIR/aligned.apk"

# Strip the existing JAR signature (the debug keystore's, from Projucer's
# generated build) before re-signing: the signature block and signature
# file, so apksigner starts from an unsigned APK rather than adding a
# second signer on top of the debug one.
cp "$INPUT_APK" "$UNSIGNED_APK"
# `zip -d` exits non-zero (and prints "zip error: Nothing to do!" to
# STDOUT, which -q does not silence) when none of the patterns match --
# true of an APK that was never signed in the first place, not an error
# here, so both streams are discarded and the exit status ignored.
zip -q -d "$UNSIGNED_APK" 'META-INF/*.SF' 'META-INF/*.RSA' 'META-INF/*.DSA' 'META-INF/*.EC' >/dev/null 2>&1 || true

nice "$ZIPALIGN" -f -p 4 "$UNSIGNED_APK" "$ALIGNED_APK"

nice "$APKSIGNER" sign \
  --ks "$KEYSTORE" \
  --ks-key-alias "$KEY_ALIAS" \
  --ks-pass "pass:$KEYSTORE_PASSWORD" \
  --key-pass "pass:$KEYSTORE_PASSWORD" \
  --out "$OUTPUT_APK" \
  "$ALIGNED_APK"

# Verification: exactly one signer, whose SHA-256 equals the keystore
# certificate's. Any other result is a failure of this script, not a
# warning -- a second signer or a mismatched key must never pass silently.
VERIFY_OUTPUT="$("$APKSIGNER" verify --print-certs "$OUTPUT_APK")"
SIGNER_SHA256_LINES="$(printf '%s\n' "$VERIFY_OUTPUT" | grep -E '^Signer #[0-9]+ certificate SHA-256 digest' || true)"
SIGNER_COUNT="$(printf '%s\n' "$SIGNER_SHA256_LINES" | grep -c . || true)"

if [ "$SIGNER_COUNT" -ne 1 ]; then
  echo "sign-apk.sh: expected exactly one signer, found $SIGNER_COUNT" >&2
  printf '%s\n' "$VERIFY_OUTPUT" >&2
  exit 1
fi

APK_SIGNER_SHA256="$(printf '%s\n' "$SIGNER_SHA256_LINES" | sed -E 's/^.*: *//' | tr 'A-F' 'a-f' | tr -d ':')"

KEYSTORE_CERT_SHA256="$("$KEYTOOL" -list -v -keystore "$KEYSTORE" -alias "$KEY_ALIAS" -storepass "$KEYSTORE_PASSWORD" \
  | grep -E '^[[:space:]]*SHA256:' | sed -E 's/^[[:space:]]*SHA256:[[:space:]]*//' | tr 'A-F' 'a-f' | tr -d ':')"

if [ -z "$KEYSTORE_CERT_SHA256" ] || [ "$APK_SIGNER_SHA256" != "$KEYSTORE_CERT_SHA256" ]; then
  echo "sign-apk.sh: signer SHA-256 ($APK_SIGNER_SHA256) does not match the keystore certificate's ($KEYSTORE_CERT_SHA256)" >&2
  exit 1
fi

echo "sign-apk.sh: $OUTPUT_APK signed by $KEY_ALIAS, one signer, SHA-256 $APK_SIGNER_SHA256 matches the keystore"
