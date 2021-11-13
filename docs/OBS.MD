# Documentation for OBS Builds

The OBS project [`home:in0rdr`](https://build.opensuse.org/project/show/home:in0rdr) comprises following `diary` packages:
* `diary`: Latest stable release built from tar.gz statically placed in the source
* `diary-nightly`: Latest unstable build, re-built dynamically from the code on the GitHub master branch (see [./CI.MD](./CI.MD) for a description of the GitHub Actions)

Additionally, these packages are being used as dependency for `diary-nightly`, to download the latest diary source code from the GitHub master branch:
* `obs-service-recompress`
* `obs-service-set_version`
* `obs-service-tar_scm`
* `zstd`

## OBS Source Services (`obs-service-`) Documentation

The documentation on how to use source services is sparse and scattered: 

* https://openbuildservice.org/help/manuals/obs-user-guide/cha.obs.source_service.html
* https://github.com/openSUSE/obs-service-tar_scm/issues/238
* https://github.com/openSUSE/obs-service-tar_scm/issues/223
* https://build.opensuse.org/package/view_file/OBS:Server:Unstable/obs-service-tar_scm/_service?expand=1

The remaining sections below describe the purpose of the source services and how they are used for the `diary-nightly` build (dependencies).

## `obs-service-recompress`

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

## `obs-service-set_version`

https://github.com/openSUSE/obs-service-set_version

