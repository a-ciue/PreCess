# License Information

> This file is the authoritative license notice for this repository.
> A Chinese translation is provided in `LICENSE.zh-CN.md` for reference only;
> in case of any discrepancy, **this English version and the verbatim
> license texts in `LGPLv3-LICENSE.txt` / `AGPLv3-LICENSE.txt` prevail**.

This repository is distributed under a **dual-license** scheme.
Different top-level directories are covered by different licenses;
please consult the sections below to determine the license that applies
to any given file.

## Section 1: LGPLv3 — Core Libraries and Build Helpers

The following directories, together with all of their sub-directories,
are licensed under the **GNU Lesser General Public License, version 3
(LGPLv3)**:

- `core/`
- `model/`
- `cmake/` (dependency-locating CMake modules; used at build time by
  both the LGPL and the AGPL parts of this repository)

The full text of the LGPLv3 is included in this repository as
`LGPLv3-LICENSE.txt`.

Under LGPLv3, third parties may link the compiled artefacts of these
directories into proprietary (closed-source) software **without having
to modify or redistribute the source of these libraries**, provided
they comply with the remaining obligations of LGPLv3. The Find modules
in `cmake/` may likewise be reused verbatim by downstream LGPL users.

No GPL-, AGPL- or otherwise LGPLv3-incompatible code may be introduced
into these directories.

## Section 2: AGPLv3 — Application, Plugins, and Everything Else

All source code in this repository that is not covered by Section 1 is
licensed under the **GNU Affero General Public License, version 3
(AGPLv3)**, including but not limited to:

- `app/`
- `plugins/`
- `test/`
- `resource/` (application branding assets and Windows resource
  scripts, shipped together with `app/`)
- Build scripts, configuration files and documentation at the
  repository root
- `...`

The full text of the AGPLv3 is included in this repository as
`AGPLv3-LICENSE.txt`.

The application and plugin layer is licensed under AGPLv3 because the
plugin dependency chain may contain third-party libraries released
under AGPLv3.

## License Boundary Rules

- **`core/`, `model/`, `cmake/` (LGPLv3)**
  - When distributed on their own, `core/` and `model/` are governed
    by LGPLv3 and may be dynamically linked into proprietary software.
  - `cmake/` is only executed at build time and is not linked into the
    resulting binaries; licensing it under LGPLv3 allows downstream
    LGPL consumers to reuse its Find modules directly.
  - These directories may be called from the AGPLv3 code in `app/` and
    `plugins/`; LGPLv3 is compatible with AGPLv3.
  - Introducing GPL-, AGPL- or otherwise LGPLv3-incompatible code into
    these directories is forbidden, as it would compromise the
    "proprietary linking" guarantee of the LGPL boundary.
- **`app/`, `plugins/`, `resource/` and all remaining directories (AGPLv3)**
  - Any modification or distribution of this code must comply with
    AGPLv3 in full, including Section 13 ("Remote Network
    Interaction"), which requires that a corresponding source
    offer be made available to all users interacting with the
    software over a network.
  - If a binary distribution bundles any AGPLv3 component, the
    distribution as a whole must be conveyed under AGPLv3.
- **Standalone LGPL derivatives**
  - A derivative work that uses only `core/`, `model/` and `cmake/`,
    and does not link against any AGPLv3 component, may be distributed
    under LGPLv3 alone and is not affected by AGPL's copyleft.
- **Adding new dependencies**
  - Dependencies added to `core/`, `model/` or `cmake/` must be
    license-compatible with **LGPLv3**.
  - Dependencies added to `app/`, `plugins/`, `resource/` or any other
    AGPL-covered area must be license-compatible with **AGPLv3**.

For more information on these licenses, see:

- [GNU Lesser General Public License v3.0](https://www.gnu.org/licenses/lgpl-3.0.html)
- [GNU Affero General Public License v3.0](https://www.gnu.org/licenses/agpl-3.0.html)