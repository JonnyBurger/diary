# Documentation for OBS Builds

The OBS project [`home:in0rdr`](https://build.opensuse.org/project/show/home:in0rdr) comprises following `diary` packages:
* `diary`: Latest stable release built from tar.gz statically placed in the source
* `diary-nightly`: Latest unstable build, re-built dynamically from the code on the GitHub master branch (see [./CI.MD](./CI.MD) for a description of the GitHub Actions)

> ℹ️ `diary-nightly` is not branched from `diary`, it has a completely separate build config.

Additionally, these packages are being used as dependency for `diary-nightly`, to download the latest diary source code from the GitHub master branch:
* [`openSUSE:Tools/obs-service-tar_scm`](https://github.com/openSUSE/obs-service-tar_scm): Use `obs_scm` to fetch latest GitHub sources

* [`openSUSE:Tools/obs-service-recompress`](https://github.com/openSUSE/obs-service-recompress): Re-compress downloaded GitHub source files
* [`openSUSE:Tools/obs-service-set_version`](https://github.com/openSUSE/obs-service-set_version): Set dynamic version based on information (`.obsinfo` file) from the GitHub checkout

## OBS Project Configuration

```
# Disable obs_scm_testsuite for obs-service-set_version:
# https://github.com/openSUSE/obs-service-set_version/issues/64
Macros:
%_without_obs_scm_testsuite 1
:Macros

# Fix "have choice for libzstd.so.1()(64bit) needed by rpm-build: libzstd libzstd1"
# https://en.opensuse.org/openSUSE:Build_Service_prjconf#Prefer
Prefer: libzstd
```

## OBS Source Services (`obs-service-`) Documentation

The documentation on how to use source services is sparse and scattered:

* https://openbuildservice.org/help/manuals/obs-user-guide/cha.obs.source_service.html
* https://github.com/openSUSE/obs-service-tar_scm/issues/238
* https://github.com/openSUSE/obs-service-tar_scm/issues/223
* https://build.opensuse.org/package/view_file/OBS:Server:Unstable/obs-service-tar_scm/_service?expand=1

The purpose of the source services and how they are used for the `diary-nightly` build (dependencies) is documented below.

### `openSUSE:Tools/obs-service-recompress`

https://github.com/openSUSE/obs-service-recompress

Branched package from [`openSUSE:Tools/obs-service-recompress`](https://build.opensuse.org/package/show/openSUSE:Tools/obs-service-recompress).

If this package is not made available in the local project `home:in0rdr`, following error message will occur:
```
nothing provides obs-service-recompress
```

No further modifications needed, but needs to be available for the `diary-nightly` for compression services:
```bash
# home:in0rdr/diary-nightly/_service
<services>
  <service mode="buildtime" name="recompress">
    <param name="file">*.tar</param>
    <param name="compression">gz</param>
  </service>
...
</services>
```

This service is used in combination with the service [`tar_scm`](https://github.com/openSUSE/obs-service-tar_scm).

### `openSUSE:Tools/obs-service-set_version`

This service sets the version in the RPM spec or Debian changelog according to the latest source files downloaded from GitHub:
https://github.com/openSUSE/obs-service-set_version

### `openSUSE:Tools/obs-service-tar_scm`

Fix build for CentOS 8 by applying this patch in `obs-service-tar_scm.spec`:
* https://github.com/openSUSE/obs-service-tar_scm/issues/374
* https://github.com/olegantonyan/obs-service-tar_scm/commit/6a454aa71395ca03a737d795551ba027a5f8a9e3

```diff
  %if %{with python3}
+
+ %if 0%{?centos_version} >= 800
+ # ignore python3 dependency (nothing provides python3 error)
+ %else
  BuildRequires:  %{use_python}%{_pkg_base}
+ %endif
```

Manual download of latest release:
https://github.com/openSUSE/obs-service-tar_scm/tags

* Use a simple filename and version, such as `obs-service-tar_scm-0.10.29.tar.gz `.
* Adjust the version numbers in the build files.

Applying the latest version fixes the following issue:
```
[    2s] Running build time source services...
[    2s] Preparing sources...
[    2s] ==> WARNING: Skipping verification of source file PGP signatures.
[    2s]     obs-service-tar_scm-0.10.28.1632141620.a8837d3.tar.gz ... Skipped
[    2s] bsdtar: Failed to set default locale
[    2s] bsdtar: Pathname can't be converted from UTF-8 to current locale.
[    2s] bsdtar: Error exit delayed from previous errors.
[    2s] ==> ERROR: Failed to extract obs-service-tar_scm-0.10.28.1632141620.a8837d3.tar.gz
[    2s]     Aborting...
[    2s] failed to prepare sources
```

The following patch might be related (needs to be applied OBS server-side, nothing left to do):
https://github.com/openSUSE/obs-build/pull/696

### `openSUSE:Factory/zstd` OBS Package

> ℹ️ This OBS package is not strictly required (no branching needed) with `tar.gz` compression in the source service (`_service`).

Patch `zstd.spec`, such that RHEL build require `glibc-static` and openSUSE builds require `glibc-devel-static`:
```diff
- BuildRequires:  (glibc-static or glibc-devel-static)
+ %if 0%{?rhel} || 0%{?fedora}
+ BuildRequires:  glibc-static
+ %else
+ BuildRequires:  glibc-devel-static
+ %endif
```

## `zstd` Compression in the ArchLinux PKGBUILD

If `diary-nightly` for ArchLinux is built with `tar.gz` compression, the build will not be shown on the [download page](https://software.opensuse.org//download.html?project=home%3Ain0rdr&package=diary-nightly) and the download button for ArchLinux is missing, see also:
* https://github.com/in0rdr/diary/issues/64
* https://github.com/openSUSE/software-o-o/issues/844
* https://github.com/openSUSE/open-build-service/pull/10570
* https://archlinux.org/news/now-using-zstandard-instead-of-xz-for-package-compression

Therefore, build the ArchLinux package with the `.pkg.tar.zst` extension/compression:

```
# ./diary-nightly/PKGBUILD
PKGEXT='.pkg.tar.zst'
```

If the ZST compression (new defacto standard since [2020-01-04](https://archlinux.org/news/now-using-zstandard-instead-of-xz-for-package-compression)) does not work, use the xz compression as an alternative as described in [openSUSE/software-o-o/#844](https://github.com/openSUSE/software-o-o/issues/844):

```
PKGEXT='.pkg.tar.xz'
```
