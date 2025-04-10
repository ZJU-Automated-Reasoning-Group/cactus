# Sparrow

Sparrow is a powerful static analysis framework designed to work with LLVM bitcode, providing advanced capabilities for call graph construction, pointer analysis, and taint analysis.

## Build Instructions

To build Sparrow, follow these steps:

```bash
cd sparrow
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

After building, the executable will be located in the `build/bin` folder. To use Sparrow, you need to compile your source code to LLVM bitcode first.

Run Sparrow with:

```bash
./sparrow your_project.bc [options]
```

## Core Functionalities

Sparrow provides several core analysis capabilities:

### 1. Call Graph Construction
- **Description**: Constructs a precise call graph by resolving indirect calls through function pointer analysis.
- **Components**:
  - `lib/FPAnalysis`: Core function pointer and call graph construction
  - `include/FPAnalysis`: Analysis interface definitions, which contains the DyckAA alias analysis and a lightweight FP analysis.

### 2. Advanced Pointer Analysis

- **Description**: Implements flow- and context-sesntiive Andersen-style pointer analysis
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

- **Function Pointer Analysis**: The default analysis performed by Sparrow.

- **Dump Transformed BC**: Remove indirect calls for better static analysis.
  ```bash
  ./sparrow your_project.bc -dump-bc
  ```
  This generates a `[input_name]_sparrow.bc` file with indirect calls replaced.

- **Dump Indirect Call Results**: Outputs indirect call targets to `indirect-call-targets.txt`.
  ```bash
  ./sparrow your_project.bc -dump-report
  ```

## Understanding the Report

The `indirect-call-targets.txt` report includes:
- File path of the call site
- Function name containing the call site
- Line number of the call site
- Number of target callees
- Function names of the target callees

## Configuration Files

Sparrow uses several configuration files in the `config/` directory:
- `ptr.config`: Configuration for pointer analysis
- `taint.config`: Sources, sinks, and propagation rules for taint analysis
- `modref.config`: Memory reference analysis configuration

## Project Structure

- `lib/`: Core analysis libraries
- `include/`: Header files for analysis libraries
- `tools/`: Command-line tools and analysis passes
- `config/`: Configuration files for various analyses
- `benchmark/`: Benchmark programs for testing
