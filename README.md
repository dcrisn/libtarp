# libtarp
Miscellaneous functions, macros, and data structures

## Building

**make**:
```
cmake -B build -DCMAKE_INSTALL_PREFIX=/home/vcsaturninus/common/playground/staging/
cd build
make
make install
```

**ninja**:
```
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/home/vcsaturninus/common/playground/staging/
cd build
ninja
ninja install
```

## todos

 * add environment variable and recipe for valgrind
 * add recipes for building a library and installing it in staging dir
 * make soversion symlinks for libraries
 * give individual mods version strings
