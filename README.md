# iris.c-scheme

A small bridge that exposes a few `iris.c` functions to `dummyscheme`, so image
generation can be driven from Scheme scripts.

- iris.c — <https://github.com/antirez/iris.c>
- dummyscheme — <https://github.com/zhuzhonghua/dummyscheme>

Neither upstream is copied or modified; both are referenced by CMake.

## Build

```sh
git clone --depth=1 https://github.com/antirez/iris.c.git
git clone --depth=1 https://github.com/zhuzhonghua/dummyscheme.git
git clone --depth=1 https://github.com/zhuzhonghua/iris.c-scheme.git

cmake -S iris.c-scheme -B build \
  -DIRIS_DIR="$PWD/iris.c" \
  -DSCHEME_DIR="$PWD/dummyscheme" \
  -DIRIS_BACKEND=mps
cmake --build build -j
```

`mps` is only an example: iris.c also supports `blas` and `generic`, and both
are accepted here as well (`auto` picks the backend for the current platform).

## Run

```sh
# 16 GB, downloaded once by the iris.c script
(cd iris.c && ./download_model.sh 4b)

mkdir -p out
./build/iris-scm ./iris.c/flux-klein-4b test.scm
```

## Scheme API

```scheme
(iris-model-info)                          ; => model description
(iris-generate prompt [width height steps seed])  ; => #t
(iris-image-info)                          ; => #(width height channels)
(iris-save path)                           ; => #t
```
