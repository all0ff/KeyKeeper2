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

## Switching between profiles (read this before you get confused)

`SDKCONFIG_DEFAULTS` only seeds `sdkconfig` the first time it's
created. Once `sdkconfig` exists in the build directory, `idf.py
build` keeps using IT, not the defaults file -- so running the Full
command, then the Lite command, in the SAME build directory produces
a second Full build, silently, no error, no warning. Confirmed: this
is exactly what happened the first time these two commands were tried
back to back here -- both builds "succeeded," both flashed, and the
Lite one still had the password generator in its menu, because it was
never actually a Lite build at all.

**Safest fix -- separate build directories, one per profile, so their
`sdkconfig` files can never collide:**

```bash
# Full
idf.py build
idf.py flash

# Lite -- own build directory via -B
idf.py -B build_lite -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;profiles/lite.defaults" build
idf.py -B build_lite flash
```

**If sharing one `build/` directory between profiles**: delete
`sdkconfig` (not the whole `build/` directory -- just that one file,
at the project root, next to this `profiles/` directory) before
switching, every time:

```bash
rm sdkconfig
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;profiles/lite.defaults" build
```

A clean way to check which profile a given build directory is
actually configured for, if ever in doubt:

```bash
idf.py -B build_lite confcheck 2>/dev/null; grep KEYKEEPER_LITE build_lite/sdkconfig
```

`CONFIG_KEYKEEPER_LITE=y` confirms Lite; no match (or `=n` / `# CONFIG_KEYKEEPER_LITE is not set`) means it's actually Full, whatever the last command typed claimed.

## Why two separate directories, not one

`profiles/` (here) and `security/profiles/` answer genuinely different
questions -- which features ship, and how hardened the chip itself
is -- and a device can be any combination of the two independently
(a Lite unit still gets Secure Boot in production; a Full dev build
still runs without it). Keeping them apart avoids a 2x2 `lite-dev`/
`lite-production`/`full-dev`/`full-production` file explosion that
would need to stay in sync by hand.

## Which flag controls what

- `CONFIG_KEYKEEPER_LITE` (`main/Kconfig.projbuild`) --
  see that file's own help text for the current list of what Lite
  strips, and why PIN hashing specifically is a real security
  trade-off while the rest are plain feature-tier differences.
- `CONFIG_SECURE_BOOT` / `CONFIG_SECURE_FLASH_ENC_ENABLED` /
  `CONFIG_NVS_ENCRYPTION` (`security/profiles/*.defaults`) -- eFuse-
  level, irreversible once burned in production mode. See
  `security/README.md`.
