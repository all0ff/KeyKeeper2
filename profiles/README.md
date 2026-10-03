# Build profiles

Two independent axes, each a plain `idf.py -D SDKCONFIG_DEFAULTS="..."`
fragment, combinable freely:

- **Feature tier** (this directory): `lite.defaults` vs. nothing
  (full is the default, no flag needed).
- **Security stage** (`security/profiles/`): `dev.defaults` vs.
  `production.defaults`. Previously undocumented anywhere -- these
  files existed with no written build command; this README is also
  the first place that command is actually written down.

## Commands

```bash
# Full, dev security (ordinary day-to-day build -- same as no flags at all)
idf.py build

# Full, dev security (explicit, identical to the above)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;security/profiles/dev.defaults" build

# Lite, dev security
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;profiles/lite.defaults;security/profiles/dev.defaults" build

# Full, production security (once security/profiles/production.defaults
# is actually filled in -- still a placeholder today, see that file's
# own comment)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;security/profiles/production.defaults" build

# Lite, production security
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;profiles/lite.defaults;security/profiles/production.defaults" build
```

`sdkconfig.defaults` (the project's own base config) must always be
listed first -- later files in the semicolon-separated list override
earlier ones for any key they both set, and the project-wide base
values still need to apply everywhere.

## Why two separate directories, not one

`profiles/` (here) and `security/profiles/` answer genuinely different
questions -- which features ship, and how hardened the chip itself
is -- and a device can be any combination of the two independently
(a Lite unit still gets Secure Boot in production; a Full dev build
still runs without it). Keeping them apart avoids a 2x2 `lite-dev`/
`lite-production`/`full-dev`/`full-production` file explosion that
would need to stay in sync by hand.

## Which flag controls what

- `CONFIG_KEYKEEPER_LITE` (`Kconfig.projbuild` at the repo root) --
  see that file's own help text for the current list of what Lite
  strips, and why PIN hashing specifically is a real security
  trade-off while the rest are plain feature-tier differences.
- `CONFIG_SECURE_BOOT` / `CONFIG_SECURE_FLASH_ENC_ENABLED` /
  `CONFIG_NVS_ENCRYPTION` (`security/profiles/*.defaults`) -- eFuse-
  level, irreversible once burned in production mode. See
  `security/README.md`.
