# Versioning & Dependencies

## Single source of truth

Library version is defined in [`VERSION`](../VERSION) at the repository root.

Before release or tagging:

```bash
python3 scripts/sync_version.py
```

CI runs `python3 scripts/sync_version.py --check` to ensure `library.properties` and `library.json` match `VERSION`.

## Release workflow

Normal development flow:

1. Merge to `main`, `master`, or `dev` through the regular CI workflow.
2. CI validates version sync, native tests, and ESP32/ESP8266 firmware builds.
3. When ready to publish, manually trigger the `Release` workflow from GitHub Actions.

Manual release requirements:

1. `VERSION`, `library.properties`, and `library.json` must already contain the release version.
2. The `version` workflow input must match `VERSION` exactly.
3. The tag `vX.Y.Z` must not already exist.

Release workflow output:

1. Runs the same quality gates as CI.
2. Creates and pushes tag `vX.Y.Z`.
3. Creates a GitHub Release.
4. If `publish_platformio=true`, publishes to PlatformIO Registry using the `PLATFORMIO_AUTH_TOKEN` GitHub secret.

Arduino Library Manager has no direct publish API. After this workflow pushes a valid `vX.Y.Z` tag, Arduino Library Manager picks it up through its registry crawler if the repo is registered there and `library.properties` is valid.

## Semver policy

This project follows [Semantic Versioning 2.0.0](https://semver.org/):

| Change | Bump | Examples |
|--------|------|----------|
| Breaking public API (macros, class methods, MQTT topic contract) | **MAJOR** | Rename `ON_ACTIVE_SESSION`, remove `pushEntity()` |
| Backward-compatible features | **MINOR** | New optional callback, new `setX()` with default |
| Bug fixes, docs, internal refactor | **PATCH** | Queue drain fix, split `ota.cpp` |

Pre-1.0 (`0.x.y`): MINOR may include small breaking changes; document them in release notes.

## `depends` in `library.properties`

Arduino Library Manager reads `depends=` to install transitive libraries automatically.

Recommended value when publishing:

```ini
depends=PubSubClient
```

| Dependency | Role in this repo |
|------------|-------------------|
| **PubSubClient** | MQTT client (vendored copy exists under `src/PubSubClient/`; declare for Arduino IDE installs that use upstream) |

Why `depends` was empty before: copy-paste install flow in README did not use Library Manager resolution. Filling `depends` helps Arduino IDE users but does not replace documenting PlatformIO `lib_deps` in README.

## PlatformIO vs Arduino IDE

- **PlatformIO**: declare versions in project `platformio.ini` `lib_deps` (recommended for reproducible CI).
- **Arduino IDE**: `depends` + Library Manager.

Keep both documented; version sync script only manages **library version**, not dependency pins.
