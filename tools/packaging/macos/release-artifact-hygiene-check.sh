#!/bin/sh
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 <manifest> <zip> [zip-listing-output]" >&2
    exit 2
fi

manifest="$1"
zip_path="$2"
listing_out="${3:-}"

test -f "$manifest" || { echo "missing manifest: $manifest" >&2; exit 1; }
test -f "$zip_path" || { echo "missing release zip: $zip_path" >&2; exit 1; }

zip_name="$(basename "$zip_path")"

grep -qx "zip=$zip_name" "$manifest" || {
    echo "manifest zip field must use artifact basename: $zip_name" >&2
    exit 1
}

grep -Eq '^sha256=[0-9a-fA-F]{64}$' "$manifest" || {
    echo "manifest missing sha256 hex digest" >&2
    exit 1
}

if grep -Eiq '(^|[=/[:space:]])(/Users/|ide_files|notary_submit\.json|APPLE_|MYIDE_AUTH_TOKEN|auth_token|token=|secret|\.pem|\.p12|\.key)([=/[:space:]]|$)' "$manifest"; then
    echo "manifest contains private/generated artifact reference" >&2
    exit 1
fi

if [ -n "$listing_out" ]; then
    mkdir -p "$(dirname "$listing_out")"
    /usr/bin/unzip -l "$zip_path" > "$listing_out"
    listing="$listing_out"
else
    listing="$(mktemp "${TMPDIR:-/tmp}/ide_release_zip_listing.XXXXXX")"
    trap 'rm -f "$listing"' EXIT HUP INT TERM
    /usr/bin/unzip -l "$zip_path" > "$listing"
fi

if grep -Eiq '(^|/)(ide_files|tmp|timerhud)(/|$)|notary_submit\.json|/\.env|\.pem|\.p12|\.key' "$listing"; then
    echo "release zip contains private/generated artifact path" >&2
    exit 1
fi

echo "release-artifact-hygiene passed."
