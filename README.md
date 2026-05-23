# cpp-ecg-streamer

Library-only C++ component that implements ECG message production/streaming (gRPC + protobuf).

This repository is intended to be used as a dependency inside the ECG-Classifier project (https://github.com/Tr0ph1c/ECG-Classifier) and not as a standalone application.

**Prerequisites**

- C++ compiler with C++17 support (e.g. `g++`)
- `make`
- `protoc` and `grpc_cpp_plugin` if you need to regenerate protobuf/gRPC sources

**Build (library)**
Build the static library used by the parent project:

```bash
make staticlib
```

This produces `libs/libecg_producer.a` and object files under `obj/static/`.

**How to use this library in a parent project**

- As a git submodule (recommended):

```bash
git submodule add https://github.com/Tr0ph1c/cpp-ecg-streamer path/to/cpp-ecg-streamer
git submodule update --init --recursive
```

- Build the library from the submodule directory (`make staticlib`) before linking.

- Link against the produced static archive and add include paths. Example `g++` link command (adjust paths):

```bash
g++ -I/path/to/cpp-ecg-streamer -L/path/to/cpp-ecg-streamer/libs \
  -lecg_producer your_consumer.cpp -o your_consumer \
  -pthread -ldl
```

Note: you may still need to link the gRPC/protobuf and other system libraries used when building the static archive (see this repo's `makefile` for `LIBS_GRPC`).

**CMake integration (example)**
If your parent project uses CMake and you added this repo as a subdirectory, you can do something like:

```cmake
add_subdirectory(path/to/cpp-ecg-streamer)
target_include_directories(my_app PRIVATE ${CMAKE_SOURCE_DIR}/path/to/cpp-ecg-streamer)
target_link_libraries(my_app PRIVATE ${CMAKE_SOURCE_DIR}/path/to/cpp-ecg-streamer/libs/libecg_producer.a)
```

**Generate protobuf / gRPC sources**
If you change `proto/ecg.proto` inside this repo, regenerate sources from the parent project or inside the submodule:

```bash
protoc --cpp_out=generated --grpc_out=generated \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) proto/ecg.proto
```

Generated files live in `generated/` (e.g. `generated/ecg.pb.h`, `generated/ecg.grpc.pb.h`).

**Usage examples (consumer code)**
Instead of running binaries in this repo, a parent project should include headers and call the library interfaces. Example consumer snippet:

```cpp
// consumer.cpp
#include "ecg_producer.hpp"   // or the headers you need from this repo

int main() {
  // create and use ECG producer/streamer API provided by the library
  // (this repo exposes compilation units; refer to ecg_producer.hpp and streamer/streamer.h)
  return 0;
}
```

Then compile and link as shown in the `g++` example above.

**Key files**

- `makefile` — build targets (see `staticlib` target)
- `ecg_producer.hpp`, `ecg_producer.cpp` — producer APIs
- `streamer/streamer.h`, `streamer/streamer.cpp` — streaming interfaces
- `proto/ecg.proto` — protobuf definitions

**Notes & next steps**

- This repository provides a static archive; ensure the parent project links any transitive dependencies (gRPC, protobuf, libcurl, system libraries) when producing a final executable.
- I can: regenerate protobufs, build the static library, or add a small CMake wrapper for easier integration — which would you prefer?
