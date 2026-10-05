# Pinned build dependencies

A checkout contains its own build dependencies. No sibling repositories or
submodule initialization are required. `dependencies.lock.json` records the
upstream commits and SHA-256 hashes of the imported files. Those hashes describe
the original snapshots; local patches remain visible in Git history.

* `third_party/routine`: Henry++ routine, commit
  `1acd9394395b7cee9e1ea7b985fb54e76be86b11`. Its GPL license and original
  copyright notices are retained. Local changes disable the legacy installer
  updater unless `APP_HAVE_UPDATES` is explicitly defined, remove certificate
  trust bypasses, propagate receive/read errors, bound memory downloads, and
  avoid dividing by zero when a memory category has no capacity.

The former sibling `builder` dependency has been replaced by the small build and
locale scripts in this repository. They do not package an installer, sign a
release, download dependencies or invoke the author's private signing setup.

The application uses the public routine APIs with their explicit section
parameters and current control/thread helpers. `python tools/build_locale.py`
regenerates the language pack without renumbering resources. Volume enumeration uses the
Windows SDK APIs instead of undeclared mount-manager types.

## Build

Install Visual Studio 2022 or 2026 (including the Build Tools edition), C++
desktop tools, a Windows SDK, and Python 3. ARM64 builds additionally require
the ARM64 C++ tools. From this checkout:

```
python tools/build.py --platform x64
python tools/build.py --platform Win32
python tools/build.py --platform ARM64
python tools/build.py --tests
```

The script discovers Visual Studio using `vswhere`; it supports v143 and v145
toolsets. Output goes to `artifacts/<platform>/<configuration>/`. The safe native
tests exercise metadata validation and the cleanup runner using fake operations.
They do not clean memory, modify volumes, or require administrator privileges.
The CI workflow builds all three architectures and runs the x64 policy tests.

## Update policy and cleanup behavior

This fork checks the fixed manifest and release URLs in `src/app.h`. Remote
metadata supplies only a bounded numeric version. HTTPS certificate failures,
redirects, invalid metadata, and failed reads abort the check. A successful
check may open the fixed release page after confirmation. It never downloads
or executes an installer and never automatically replaces locale files.
Published binaries still need a verified publisher signature before manual
installation; this change does not implement a signed automatic updater.

Cleanup runs in one worker at a time. The UI stays responsive, and completion
reports successful, failed, and unsupported/cancelled operations separately.
The available-memory delta is an observation during the job, not a measurement
of memory freed exclusively by this process. Automatic cleanup uses a monotonic
cooldown and cannot flush whole volumes. Expensive standby/modified-list cleanup
still requires the existing opt-in; volume flushing is available only through
explicit manual selection. These changes reduce blocking and repeated work;
they do not guarantee faster applications or benchmarked performance gains.
