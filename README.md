FileSort &mdash; fast file sorting utility
============================

## Build

Use GCC 10 or Clang 10 or later.

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

## Large file generator

```sh
# generate a 50 Gb file of lines from 1 to 1024 characters
./fgen in.txt 1024 50000000000
```

## File sorter

```sh
# sort in.txt and output result to out.txt
./fsort in.txt out.txt

# advice the application to use 50000 memory pages simmutaneously
# and sort
./fsort in.txt out.txt 50000
```

