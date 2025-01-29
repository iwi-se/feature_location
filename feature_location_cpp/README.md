# Feature Location

## Install dependencies

```bash
conan profile detect --force
conan install . --output-folder=build --build=missing

# With debug information
conan install . --output-folder=build --build=missing --profile=debug # Debug profile must be created. Just copy the default profile and change Release to Debug.
```

## Build

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
```
