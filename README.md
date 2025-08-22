# myJVMinCpp

This is a simple JVM (Java Virtual Machine) implemented in C++ that supports basic class file parsing and bytecode interpretation and execution.

## Build(CMake)

```sh
mkdir build
cd build
cmake ..
make
```

## Usage

Put the class file (e.g `Test.class`) into the location where your build, run

```sh
./myJVMinCpp Test.class
```

If the class file is not specified, load `Test.class` defaultly.

## Features

- Supports basic parsing of Java class files (constant pool, methods, attributes, etc.)
- Supports most interpretation and execution of some mainstream bytecode instructions (iconst, iload, istore, iadd, return, etc.)
- Supports class searching and loading
- Basic runtime structures (stack frame, local variable table, operand stack, simple object model)

## Plan

- Implements all bytecode instructions
- Implements more native methods (JNI)
- Supports exception handling
- Supports arrays
- Supports multithreading and synchronization mechanisms
- Supports GC
- Improves the object model and heap management

---

If you have any suggestions or questions, please submit an issue or PR!