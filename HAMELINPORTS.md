# HamelinPorts ImsMedia fork

This is a [HamelinPorts](https://github.com/HamelinPorts) fork of AOSP's
`packages/modules/ImsMedia`, used by `HamelinPortsImsService` to drive
VoLTE / VoWiFi media on devices where stock Samsung IMS is replaced.

Upstream lives at:
[`android.googlesource.com/platform/packages/modules/ImsMedia`](https://android.googlesource.com/platform/packages/modules/ImsMedia)

## Branches

| Branch          | Purpose                                                          |
|-----------------|------------------------------------------------------------------|
| `lineage-23.2`  | **Default.** Our patched branch tracked by the lineage manifest. |
| `main`          | Mirror of upstream `main`. Read-only; do not commit here.        |
| `aml_*`, etc.   | Mirror copies of all upstream branches (for reference).          |

## What we patch

Four commits on top of upstream `android-16.0.0_r4`, all in `libimsmedia`:

* **`fix uplink AMR-WB on HALs whose burst != ptime`** — caps the AAudio
  capture read to one ptime frame (`mSamplingRate * mPtime / 1000`).
  Upstream reads `AAudioStream_getFramesPerBurst()` per poll; with a 40 ms
  HAL burst (Unisoc UMS512 AGDSP VoIP scene) against a 20 ms ptime,
  MediaCodec emits two AMR-WB frames per buffer, the per-frame payload
  encoder maps them to NoData and the uplink is silent. Also makes
  `ImsMediaBitWriter::Write()` OR-merge spilled bytes so the header and
  payload writers sharing a byte stop clobbering each other.
* **`clear AMR depacketizer payload buffer before each frame`** — the AMR
  and EVS depacketizers reuse `mPayload` without re-zeroing; with the
  OR-merging BitWriter, consecutive ToC bytes accumulate into an illegal
  frame type and the downlink decoder stops.
* **`tolerate AAudio waitForStateChange timeout in Start()`** — a start
  timeout means the HAL is slow, not that the stream failed; keep the
  codec instead of tearing it down (which silenced the downlink for the
  rest of the call).
* **`open AAudio streams SHARED so the policy keeps routing`** — opens
  both streams SHARED / `PERFORMANCE_MODE_NONE` so voice calls stay on
  AudioFlinger's normal path and follow `setCommunicationDevice()`
  mid-call. No effect on devices without MMAP; defensive for those that
  enable it.

Drop each commit once its fix lands upstream; when none is left, reset
`lineage-23.2` to the upstream branch we want to track.

## Rebasing onto a newer AOSP release

Upstream's `main` branch is rolling; AOSP releases are immutable tags
(`android-16.0.0_r4`, `android-16.0.0_r5`, …). The Monday 06:00 UTC
GitHub Actions workflow (`.github/workflows/sync-upstream.yml`) attempts
a **rebase onto `upstream/main`** weekly and opens a `rebase-conflict`
issue if it cannot. To do it manually, e.g. when bumping to a specific
release tag:

```sh
git remote add upstream https://android.googlesource.com/platform/packages/modules/ImsMedia
git fetch upstream
git checkout lineage-23.2

# Choose your target. Use the upstream tag that matches the release
# the lineage-23.2 manifest is shipping at the moment, or the latest
# r-tag for that platform line. Inspect with `git tag | grep '^android-16'`.
git rebase android-16.0.0_r5     # example

# Resolve any conflicts in
#   service/src/com/android/telephony/imsmedia/lib/libimsmedia/core/audio/android/ImsMediaAudioSource.cpp
# (most upstream changes do not touch the read loop, so this should be
# rare). git add the resolved files and:
git rebase --continue

# Build-test before publishing — the fix is small and easy to break.
# From the lineage tree:
#   m libimsmedia
# Then place a VoLTE call and confirm uplink TX packets are
# 12 RTP hdr + 33-60 byte AMR-WB payload (NOT 14-byte FT=14 frames).

git push --force-with-lease origin lineage-23.2
```

If you bumped past a known-good tag and want to capture that as a new
"good" point, also push a tag:

```sh
git tag hamelinports/android-16.0.0_r5 lineage-23.2
git push origin hamelinports/android-16.0.0_r5
```

## Manifest entry

A device's local manifest replaces the AOSP project with this fork, e.g.
in `.repo/local_manifests/gta8.xml`:

```xml
<remove-project name="platform/packages/modules/ImsMedia" />
<project path="packages/modules/ImsMedia"
         name="android_packages_modules_ImsMedia"
         remote="gta8-hamelin" />
```

The remote (`https://github.com/HamelinPorts/`) carries
`revision="lineage-23.2"`, so `repo sync` picks up our patched branch by
default.

## Reporting upstream

These fixes should eventually land in AOSP gerrit. Once all have,
this fork can be dropped entirely and the manifest reverted to AOSP. Until
then, keep the commit message clear, the rationale linkable, and the
patch small enough that an AOSP reviewer can see it as a one-liner-ish
correctness fix.
