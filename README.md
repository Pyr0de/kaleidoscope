# Kaleidoscope

A small programming language and compiler implementation built using LLVM.

[LLVM Kaleidoscope Tutorial](https://llvm.org/docs/tutorial/index.html)


## Example

#### **`fib.ks`**
```
extern printd(x);

def fib(x)
  if x < 3 then
    1
  else
    fib(x - 1) + fib(x - 2);

printd(fib(10));
```

Build Kaleidoscope Compiler
```sh
mkdir build && cd build
cmake ..
make
```

Compile Kaleidoscope code
```sh
./kaleidoscope < fib.ks
clang output.o ../lib/lib.cpp -o out
./out
```
