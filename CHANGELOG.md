# Changelog

## 1.2.2 - 2026-10-27

- Stop subscribing to the retired `le` and `nl` MQTT topics; typed usage callbacks now handle only `ur`.
- Keep `ON_LIMIT_EXCEEDED` and `ON_NEAR_LIMIT` as deprecated callback-definition macros that register nothing. Keep the usage flag setters and existing enum values for source compatibility.
- Publish example entity state after `ON_INIT` and only on subsequent changes. Remove repeated unchanged state, synthetic uptime heartbeats, and random sensor traffic; keep sensor sampling and image capture separate from entity publishing.
- Correct invalid batch `.send()` calls and a stray character in the sensor sketch. Synchronize both library manifests with the canonical version.
