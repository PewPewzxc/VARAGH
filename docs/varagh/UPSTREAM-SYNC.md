# Keeping VARAGH up to date with CrossInk and CrossPoint

VARAGH 1.0.0 is based on CrossInk v1.6.0 (commit `b25beb13`). The repository has
three remotes set up for this:

| Remote | Repository | Use |
|---|---|---|
| `origin` | github.com/PewPewzxc/varagh | your fork (push here) |
| `upstream` | github.com/uxjulia/crossink | CrossInk, merge its releases |
| `crosspoint` | github.com/crosspoint-reader/crosspoint-reader | pick single fixes/features |

The display/UI library is a submodule pointing to your fork
github.com/PewPewzxc/freeink-sdk (branch `varagh`), whose own `upstream` remote is
github.com/Free-Ink/freeink-sdk.

## Merge a new CrossInk release

```bash
git fetch upstream --tags
git checkout -b sync/crossink-v1.7.0 main
git merge v1.7.0            # or: git merge upstream/main
```

Conflicts are most likely in files VARAGH changed (translations, the dictionary
and reader activities, settings). Keep both sides where you can; the list of
VARAGH changes is in the [changelog](../../CHANGELOG.md).

If CrossInk moved the `freeink-sdk` submodule to a newer commit, update your SDK
fork the same way:

```bash
cd freeink-sdk
git fetch upstream
git checkout varagh
git merge <commit CrossInk points to>   # shown by: git -C .. diff upstream/main -- freeink-sdk
git push origin varagh
cd ..
git add freeink-sdk
```

Then build, test on the device, and merge into `main`:

```bash
pio run -e x4-pro
git checkout main && git merge sync/crossink-v1.7.0 && git push
```

After a merge, check the three VARAGH-specific settings still hold:
`CROSSINK_OTA_RELEASE_URL` in `src/network/OtaUpdater.cpp` points to
`PewPewzxc/varagh`, `version` in `platformio.ini` is VARAGH's version, and the
`STR_CROSSINK` strings still say VARAGH.

## Take one feature or fix from CrossPoint

CrossPoint and CrossInk have drifted apart, so take single pull requests rather
than merging everything:

```bash
git fetch crosspoint
git cherry-pick -x <commit>          # a commit from a merged PR
```

If a cherry-pick does not apply, fetch the PR's diff
(`https://github.com/crosspoint-reader/crosspoint-reader/pull/<N>.diff`), apply
it by hand, and note "ported from CrossPoint #N" in the commit message and
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md). VARAGH 1.0.0 already
contains CrossPoint #3616 (shared font interval tables) and #3633 (faster glyph
drawing).

## Release

1. Raise `version` in `platformio.ini` (for example `1.0.1`) and add a
   changelog entry.
2. `pio run -e x4-pro`, test on the device, commit and push.
3. On GitHub: **Releases → Draft a new release**, tag `v1.0.1`, attach
   `.pio/build/x4-pro/firmware-x4-pro.bin` **with exactly that file name**
   (online updates look for it), the SD-card zip and a full-source zip.
