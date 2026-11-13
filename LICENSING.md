# Licensing

## Project Cars license

Project Cars original code, documentation, and assets that the project's rights
holders are entitled to license are declared under the **GNU General Public
License, version 3 only**.

SPDX identifier: `GPL-3.0-only`.

The full, unmodified license text is in [LICENSE](LICENSE). The license includes
its warranty disclaimer. "Version 3 only" is intentional; this declaration does
not add an "or any later version" grant.

## Packaged binary's corresponding source remains unestablished

The imported runtime is a compiled native C++ DLL identified as 0.8.16.
Recovered upstream 0.8.19 source, its full GPLv3 license and an isolated build
are now preserved under `original`; see [original/README.md](original/README.md)
for provenance and dependencies. This does not establish that this revision
produced the imported binary. A PDB is not corresponding source, and the
uploaded legacy PDB's GUID does not match the packaged DLL.

GPLv3 has requirements for conveying object code and making its complete
Corresponding Source available; see sections 1 and 6 of [LICENSE](LICENSE).
Adding this license file or describing a binary in the wiki does not fulfill
those requirements. No complete source package or compliant source offer is
provided by the current upload.

Before treating the native binary distribution as GPLv3-compliant, the project's
rights holders must resolve the applicable source-distribution requirements,
including the source and build materials needed for the distributed build.
Do not present this repository as a complete, reproducible GPL source release.

## Recovered source and independent remake

The immutable recovered snapshot comes from Teirdalin/Project-Cars commit
`740a9961c2ae47c75013fab5402189abca459354` under its explicit
**GPL-3.0-only** declaration. Its upstream license and compatibility attribution
are retained. The local build wrapper and source manifest do not relicense
external engine/SDK/compiler dependencies or establish rights to omitted assets.
The earlier 0.8.16 source publication is not a proven matching build either.

The C++ catalog parser, standalone vehicle/fuel/serialization core,
development-only bootstrap, tests, scripts and documentation under `remake`
are new original contributions under **GPL-3.0-only**. Their source
and pinned build instructions are provided, and their separate output is named
`ProjectCarsRemake.dll`; external SDK/compiler dependencies are not vendored.

This partial reimplementation is not the Corresponding Source for the unrelated
legacy `ProjectCars.dll`, and does not resolve the legacy binary's source
distribution requirements. It implements no drivable gameplay and is not a
deployment-ready mod or an unrestricted binary/asset release. Dependencies and
any future distribution still require their applicable licenses and notices.

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
