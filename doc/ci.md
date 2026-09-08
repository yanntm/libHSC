# The CI: published binaries

`.github/workflows/linux.yml` and `osx.yml` run on every push to `master`.
Each runs `build_hsc.sh` at the repository root (a static libexpat, GMP from
the runner's package manager, or built from source when absent, then libHSC
with `HSC_STATIC=ON`, the test suite as a gate, stripped binaries in
`website/`) and deploys `website/` to a branch of this repository:

| runner | branch | linking |
|---|---|---|
| `ubuntu-24.04` | `HSC-Linux` | fully static |
| `macos-15` | `HSC-OSX` | dynamic against the system libc++; not maintained, may lag or fail: consumers take Linux only |

Published tools: `hsc`, `hsc-pn`, `hsc-mcc`, `nupn2hsc`, `dve2hsc`, `fsp2hsc`. A binary
is fetched at

```
https://github.com/yanntm/libHSC/raw/HSC-Linux/hsc
```

Consumers: the MCC harness driver (`~/git/MCC-drivers/hsc/install.sh`) and
the ITS-Tools plugin `fr.lip6.hsc.binaries`, which downloads the binaries at
Maven build time. Hence the order when a Java change needs a new flag: push
here, wait for the deploy, then push ITS-Tools.

## Checking a run

```
curl -s "https://api.github.com/repos/yanntm/libHSC/actions/runs?per_page=4" | python3 -c "
import sys,json
for w in json.load(sys.stdin)['workflow_runs']: print(w['created_at'], w['status'], w['conclusion'], w['head_sha'][:8], w['name'])"
git fetch origin HSC-Linux && git log -1 --format='%h %ci %s' origin/HSC-Linux
```

A failed run's log is under the run's `jobs_url` and needs a token to read
in full; the summary line names the failing step.

## Reproducing the Linux build locally

`./build_hsc.sh` reproduces the run on the host (its own tree
`build-static/`, prefix `usr/local/`, both git-ignored). The runner's
compiler is Ubuntu 24.04's GCC 13, older than a developer's; a podman
container checks that before a push:

```
podman run --rm -v "$PWD":/src:Z -w /src ubuntu:24.04 bash -c \
  'apt-get -qq update && apt-get -qq install -y build-essential cmake git wget libgmp-dev > /dev/null && bash build_hsc.sh'
```

The container writes into the mounted tree (`build-static/`, `usr/`,
`website/`, `deps/`); remove them or reuse them as the host script would.
