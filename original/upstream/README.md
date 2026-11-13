# Project Cars for RE_Kenshi

Project Cars adds modular drivable vehicles to Kenshi. The native plugin runs under
[RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) and links against
[KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib).

## Build

Use Visual C++ 2010 x64, Windows SDK 7.1, and the
[KenshiLib example dependencies](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps).
The build script lists the expected toolchain layout. Pass the parent directory
that contains `toolchain` and `KenshiLib_Examples_deps`.

```powershell
.\tools\build.ps1 -KenshiProjects 'C:\path\to\dependencies'
```

The script emits a Release x64 DLL and PDB under `build/`. Running the plugin
requires a compatible RE_Kenshi installation and matching Project Cars game data
and assets.

## License

Project Cars source is licensed under [GNU GPL version 3 only](LICENSE).
The adapted KenshiLib header retains its upstream provenance in
[plugin/compat/README.md](plugin/compat/README.md). External dependencies retain
their respective licenses.
