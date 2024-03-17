# fuse-tmpfs

## Requirments

1. libfuse-dev
2. pkg_config
3. cmake
4. gcc

## Build

1. ```mkdir build && cd build```
2. ```cmake .. && make```

## Usage

1. ```cd ../example```
2. ```../build/fuse-tmpfs mountdir/```
3. Use mountdir as ordinary directory
4. To read log in real-time `tail -F tmpfs.log`
5. To unmount directory `fusermount -u {PROJECT_DIR}/example/mountdir`