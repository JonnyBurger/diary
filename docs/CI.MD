# Documentation of CI Jobs

## OpenSuse Build Service Nightly Builds from Master
`push` events on the `master` branch trigger a rebuild of the [`diary-nightly`](https://build.opensuse.org/package/show/home:in0rdr/diary-nightly) package in the OpenSuse Build Service.


These jobs are called "workflows" in the OpenSuse Build Service and they are stored at [`.obs/workflows.yml`](../.obs/workflows.yml).

The documentation for this kind of "workflow" can be found in the [GitHub repository of the Open Build Service](https://github.com/openSUSE/open-build-service/wiki/Better-SCM-CI-Integration) and in a [series of blog posts on "SCM integration".](https://openbuildservice.org/blog).

## GitHub Action to Verify Build

[`.github/workflows/c.yml`](../.github/workflows/c.yml) implements a GitHub action to run the simple series of build steps (`make && make install`) for new commits and pull requests on the `master` branch.

[`.github/c/make-install`](.../.github/c/make-install) is a re-usable action (dependency).

## GitHub Action to Render Man Page

[`.github/workflows/man.yml`](../.github/workflows/man.yml) is a job to render a new version of the man page after each commit or pull request on the `master` branch.

This file is consumed by a Terraform job and rendered at https://diary.in0rdr.ch/man.

A refresh of the html file in this GitHub repository does not automatically refresh the man page on the website (manual step).