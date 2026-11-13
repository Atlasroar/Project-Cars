# Local VC10 header compatibility

`NavMesh.h` is copied from
`../KenshiLib_Examples_deps/KenshiLib/Include/kenshi/NavMesh.h` relative to the
Project Cars root. It retains the upstream declarations and layout. The single
change replaces `MessageQueue<T>::Node*` with `Node*` inside the `MessageQueue`
template, where VC10 rejects the redundant qualification.

Do not edit the shared dependency to build this project. Preserve KenshiLib's
license and distribution requirements with the rest of the native dependencies.
Recheck this local copy whenever updating KenshiLib; the adapter is tied to the
inspected 0.5.0 headers and Steam Kenshi 1.0.65 ABI.
