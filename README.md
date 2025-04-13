# Cactus

Cactus is a powerful static analysis framework designed to work with LLVM bitcode, providing advanced capabilities for call graph construction, pointer analysis, and taint analysis.

## Build Instructions

To build Cactus, follow these steps:

```bash
cd cactus
mkdir build && cd build
cmake ../ -DLLVM_BUILD_PATH=[LLVM3.6.2 Build Path]
make -j$(nproc)
```

Replace `[LLVM3.6.2 Build Path]` with the path to your LLVM 3.6.2 build directory.

## Requirements

- LLVM 3.6.2
- C++14 compliant compiler
- Python 3
- CMake 3.4.3 or newer

## Usage

After building, the executables will be located in the `build/bin` folder. 

## Available Tools

Cactus provides several specialized analysis tools:

### Sparrow
A call graph construction tool focusing on resolving indirect calls through function pointer analysis.

```bash
./sparrow your_project.bc [options]
```

Options:
- `-dump-bc`: Dumps the transformed bitcode with resolved indirect calls
- `-dump-report`: Outputs indirect call targets to `indirect-call-targets.txt`

### WPA (Whole Program Analysis)
Implements pointer analysis algorithms from the SVF framework.

```bash
./wpa your_project.bc [options]
```

### SABER
Static bug checker for memory issues like leaks, use-after-free, and double-free problems.

```bash
./saber your_project.bc [options]
```

### taint-check
Performs taint analysis to identify potential security vulnerabilities.

```bash
./taint-check your_project.bc [options]
```

### Other Tools
- `global-pts`: Global points-to analysis
- `pts-dump`: Points-to information dump
- `pts-inst`: Points-to instrumentation
- `pts-verify`: Points-to verification
- `vkcfa-taint`: Value and context flow-aware taint analysis
- `table` and `table-check`: Analysis table generation and verification

## Core Functionalities

Cactus provides several core analysis capabilities:

### 1. Call Graph Construction (Sparrow)
- **Description**: Constructs a precise call graph by resolving indirect calls through function pointer analysis.
- **Components**:
  - `lib/FPAnalysis`: Core function pointer and call graph construction
  - `include/FPAnalysis`: Analysis interface definitions, which contains the DyckAA alias analysis and a lightweight FP analysis.

The `indirect-call-targets.txt` report includes:
- File path of the call site
- Function name containing the call site
- Line number of the call site
- Number of target callees
- Function names of the target callees


### 2. Advanced Pointer Analysis

- **Description**: Implements flow- and context-sensitive Andersen-style pointer analysis
- **Components**:
  - `lib/PointerAnalysis`: Pointer analysis engine with multiple precision levels (flow, context)
  - `include/PointerAnalysis`: Analysis interface definitions

### 3. SVF-based Pointer Analyses
- **Description**: Incorporates SVF (Static Value-Flow Analysis) framework for precise pointer analysis
- **Components**:
  - `lib/SVF/WPA`: Various pointer analysis implementations including:
    - Andersen's inclusion-based analysis
    - Wave propagation optimization for Andersen's
    - Steensgaard's unification-based analysis
    - Flow-sensitive analysis
  - `lib/SVF/SABER`: Static bug checkers for memory issues like:
    - Memory leaks
    - Use-after-free
    - Double-free
  - `include/SVF`: Interface definitions for SVF analyses

### 4. Taint Analysis Framework
- **Description**: Tracks the flow of potentially malicious or sensitive data throughout the program.
- **Components**:
  - `lib/TaintAnalysis`: Taint propagation engine
  - `include/TaintAnalysis`: Taint analysis interface
  - `config/taint.config`: Default taint configuration

Use with `-taint-config [file]` to specify a custom configuration file.

### 5. Dynamic Analysis Support
- **Description**: Enables runtime verification and hybrid static-dynamic analyses
- **Components**:
  - `lib/Dynamic/Analysis`: Runtime pointer analysis and memory tracking
  - `lib/Dynamic/Instrument`: Program instrumentation for runtime monitoring
  - `lib/Dynamic/Runtime`: Runtime libraries for dynamic analysis
  - `lib/Dynamic/Log`: Log processing for dynamic execution traces
  - `include/Dynamic`: Interface definitions for dynamic analyses

### 6. Numerical Analysis
- **Description**: Performs numerical range analysis to determine possible value ranges for variables
- **Components**:
  - `lib/Numerics/RangeAnalysis`: Implementation of intra-procedural range analysis
  - `include/Numerics`: Interface definitions for numerical analyses

## Command Line Options

Cactus tools support various command line options:

### Common Options
- `-config-file [path]`: Specify a configuration file
- `-help`: Display available options
- `-stats`: Print analysis statistics

### Sparrow Options
- `-dump-bc`: Dump transformed bitcode
- `-dump-report`: Dump indirect call information

### Taint Analysis Options
- `-taint-analysis`: Enable taint analysis
- `-taint-config [file]`: Specify custom taint configuration

### Pointer Analysis Options
- `-ander`: Use Andersen's pointer analysis
- `-sfs`: Use flow-sensitive pointer analysis
- `-wpa`: Enable whole program analysis

## Configuration Files

Cactus uses several configuration files in the `config/` directory:
- `ptr.config`: Configuration for pointer analysis
- `taint.config`: Sources, sinks, and propagation rules for taint analysis
- `modref.config`: Memory reference analysis configuration

## Project Structure

- `lib/`: Core analysis libraries
- `include/`: Header files for analysis libraries
- `tools/`: Command-line tools and analysis passes
  - `Sparrow/`: Call graph construction
  - `WPA/`: Whole program analysis
  - `SABER/`: Memory bug detection
  - `taint-check/`: Taint analysis
  - And many more specialized tools
- `config/`: Configuration files for various analyses
- `benchmark/`: Benchmark programs for testing
