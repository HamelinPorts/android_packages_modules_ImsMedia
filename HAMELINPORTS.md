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

Currently one commit on top of upstream `android-16.0.0_r4`:

* **`libimsmedia: cap AAudio capture read to one ptime frame`** — fixes
  uplink audio on devices whose audio HAL exposes a capture burst larger
  than the negotiated codec ptime (e.g. Unisoc UMS512, where the AGDSP
  VoIP scene runs at a 40 ms period). The upstream code blindly uses
  `AAudioStream_getFramesPerBurst()` as the per-poll read size, which
  causes MediaCodec to emit two concatenated AMR-WB frames per output
  buffer; `AudioRtpPayloadEncoderNode::EncodePayloadAmr` is a per-frame
  parser and falls through to `kImsAudioAmrWbModeNoData` for any size
  that does not match a single-frame entry in `gaAMRWBLen[]`. Result: 14-byte
  RTP payloads with `FT=14` (SPEECH_LOST) — the far end hears silence on
  every uplink frame. The fix caps the read to `mSamplingRate * mPtime / 1000`.

If this patch ever lands upstream, drop the commit and reset
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

This repo is wired into the LineageOS build via
`.repo/local_manifests/x205-ims.xml` in the device tree. The relevant
project line is:

```xml
<project path="packages/modules/ImsMedia"
         name="android_packages_modules_ImsMedia"
         remote="hamelinports" />
```

The `hamelinports` remote already has `revision="lineage-23.2"`, so
nothing else is needed in the manifest — `repo sync` picks up our patched
branch by default.

## Reporting upstream

The read-chunk fix should eventually land in AOSP gerrit. When it does,
this fork can be dropped entirely and the manifest reverted to AOSP. Until
then, keep the commit message clear, the rationale linkable, and the
patch small enough that an AOSP reviewer can see it as a one-liner-ish
correctness fix.
