# Licensing

## Project Cars license

Project Cars original code, documentation, and assets that the project's rights
holders are entitled to license are declared under the **GNU General Public
License, version 3 only**.

SPDX identifier: `GPL-3.0-only`.

The full, unmodified license text is in [LICENSE](LICENSE). The license includes
its warranty disclaimer. "Version 3 only" is intentional; this declaration does
not add an "or any later version" grant.

## Missing corresponding source

The imported runtime is a compiled native C++ DLL. This repository does not
contain its C++ source, headers, build project, or the scripts and instructions
needed to rebuild it. A PDB is not corresponding source, and the uploaded PDB's
GUID does not match the current DLL.

GPLv3 has requirements for conveying object code and making its complete
Corresponding Source available; see sections 1 and 6 of [LICENSE](LICENSE).
Adding this license file or describing a binary in the wiki does not fulfill
those requirements. No complete source package or compliant source offer is
provided by the current upload.

Before treating the native binary distribution as GPLv3-compliant, the project's
rights holders must resolve the applicable source-distribution requirements,
including the source and build materials needed for the distributed build.
Do not present this repository as a complete, reproducible GPL source release.

## Third-party and unidentified material

This declaration does not relicense Kenshi, RE_Kenshi, KenshiLib, Ogre, MyGUI,
Microsoft runtime libraries, or other independently licensed dependencies.
References to game-installed meshes and textures do not grant redistribution
rights to those files. The imported backups and caches do not establish asset
ownership or licensing rights.

The [V8 audio manifest](audio/v8/manifest.json) identifies a user-supplied archive
but records both `author` and `license` as `null`. The samples' redistribution
rights and compatibility with the intended distribution therefore remain
unverified. They are not asserted to be newly relicensed or cleared by the
Project Cars GPL declaration. Obtain provenance and permission before assuming
they may be redistributed.

Original third-party notices and applicable licenses must be retained.
Other asset provenance should also be reviewed before a public release; a
checksum verifies identity, not authorship or permission.
