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
Implements several pointer analysis algorithms from the SVF framework.

```bash
./wpa your_project.bc [options]
```

### SABER
Static bug checker for memory issues like leaks, use-after-free, and double-free problems (from SVF).

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
- `pts-dump`: Points-to information dump utility that performs context-sensitive points-to analysis on LLVM bitcode and displays analysis results
  ```bash
  ./pts-dump input.bc [options]
  ```
  Options:
  - `-ptr-config <file>`: Specify an annotation file for external library points-to analysis (default: ptr.config)
  - `-k <number>`: Set the context sensitivity level (k-limiting) for the analysis (default: 1)
  - `-no-prepass`: Skip IR canonicalization before analysis
  - `-dump-pts`: Dump detailed points-to sets after analysis (otherwise only summary statistics are shown)
  - `-debug-context`: Enable additional debug output for context sensitivity issues
- `pts-inst`: Points-to instrumentation
- `pts-verify`: Points-to verification
- `vkcfa-taint`: Value and context flow-aware taint analysis
- `table` and `table-check`: Analysis table generation and verification
- `dot-du-module`: Dot graph generator for def-use chains
- `dot-pointer-cfg`: Dot graph generator for pointer CFGs

## Common Command-Line Options

Most Cactus tools support the following common options:

```bash
-ptr-config <file>      # Specify a custom ptr.config file
-taint-config <file>    # Specify a custom taint.config file 
-modref-config <file>   # Specify a custom modref.config file
-dot-callgraph          # Generate a DOT file of the call graph
-dump-ir                # Dump the transformed IR
-verbose                # Enable verbose output
-stats                  # Print analysis statistics
```

Each tool may have additional specific options. Use `-help` with any tool to see available options.

## Core Components

Cactus provides several core analysis capabilities:

### 1. Alias Analysis Collection
- **Description**: A comprehensive collection of alias analysis implementations.
- **Components**:
  - `lib/Alias/`: Contains various alias analysis implementations:
    - **SRAA (Strict Relation Alias Analysis)**: Identifies non-aliasing pointers by analyzing their strict mathematical relationships
      - Includes range analysis for determining variable bounds
      - Value-based Static Single Assignment form implementation
    - **OBAA (Offset-Based Alias Analysis)**: Leverages offset information to determine alias relationships
    - **Canary**: A lightweight, Dyck-reachability based alias analysis


### 2. Call Graph Construction (Sparrow)
- **Description**: Constructs a precise call graph by resolving indirect calls through type and pointer analysis.
- **Components**:
  - `lib/FPAnalysis`: Core function pointer and call graph construction
  - `include/FPAnalysis`: Analysis interface definitions

The `indirect-call-targets.txt` report includes:
- File path of the call site
- Function name containing the call site
- Line number of the call site
- Number of target callees
- Function names of the target callees

### 3. Advanced Pointer Analysis
- **Description**: Implements flow- and (k-limiting) context-sensitive Andersen-style pointer analysis
- **Components**:
  - `lib/PointerAnalysis`: Pointer analysis engine with multiple precision levels (flow, context)
  - `include/PointerAnalysis`: Analysis interface definitions

### 4. SVF-based Pointer Analyses
- **Description**: Incorporates SVF (Static Value-Flow Analysis) framework for precise pointer analysis
- **Components**:
  - `lib/SVF/WPA`: Various pointer analysis implementations including:
    - Andersen's inclusion-based analysis
    - Wave propagation optimization for Andersen's
    - Steensgaard's unification-based analysis
    - Flow-sensitive, Andersen-style analysis
  - `lib/SVF/SABER`: Static bug checkers for memory issues like:
    - Memory leaks
    - Use-after-free
    - Double-free
  - `include/SVF`: Interface definitions for SVF analyses

### 5. Taint Analysis Framework
- **Description**: Tracks the flow of potentially malicious or sensitive data throughout the program, which relies on the pointer analysis in `lib/PointerAnalysis`.
- **Components**:
  - `lib/TaintAnalysis`: Taint propagation engine
  - `include/TaintAnalysis`: Taint analysis interface
  - `config/taint.config`: Default taint configuration

Use with `-taint-config [file]` to specify a custom configuration file.

### 6. Dynamic Analysis Support
- **Description**: Enables runtime verification and hybrid static-dynamic analyses
- **Components**:
  - `lib/Dynamic/Analysis`: Runtime pointer analysis and memory tracking
  - `lib/Dynamic/Instrument`: Program instrumentation for runtime monitoring
  - `lib/Dynamic/Runtime`: Runtime libraries for dynamic analysis
  - `lib/Dynamic/Log`: Log processing for dynamic execution traces
  - `include/Dynamic`: Interface definitions for dynamic analyses

## Configuration Files

Cactus uses several configuration files in the `config/` directory:
- `ptr.config`: Configuration for pointer analysis, specifying memory allocation and access behaviors
- `taint.config`: Sources, sinks, and propagation rules for taint analysis
- `modref.config`: Memory reference analysis configuration, specifying function read/write effects

### Configuration File Formats

#### ptr.config
This file specifies pointer behavior of library functions:
```
fopen ALLOC                   # Allocates memory
getcwd COPY Ret V Arg0 V      # Copies from Arg0 to return value
getpwuid COPY Ret V STATIC    # Returns a pointer to static memory
IGNORE chmod                  # Function call can be ignored in pointer analysis
```

#### taint.config
This file defines taint sources, sinks, and propagation rules:
```
SOURCE fgetc Ret V T          # Return value is a taint source
SINK printf Arg0 D            # First argument is a taint sink
PIPE memcpy Arg0 R Arg1 R     # Taint propagates from Arg1 to Arg0
IGNORE free                   # Function call can be ignored in taint analysis
```

#### modref.config
This file specifies memory effects of library functions:
```
# Format examples:
malloc REF_ARG NONE           # malloc references its arguments but has no mod effects
strcpy MOD_ARG REF_ARG        # strcpy modifies and references its arguments
free MOD_ARG NONE             # free modifies its arguments
```

## Project Structure

- `lib/`: Core analysis libraries
  - `Alias/`: Collection of alias analysis implementations (Andersen's, DyckAA, Type-based, SRAA, OBAA, Canary)
  - `FPAnalysis/`: Function pointer analysis implementation
  - `PointerAnalysis/`: Pointer analysis implementation
  - `SVF/`: Static Value-Flow analysis framework
  - `TaintAnalysis/`: Taint analysis implementation
  - `Dynamic/`: Dynamic analysis support
  - `Annotation/`: Code annotation utilities
  - `Transforms/`: LLVM IR transformations
  - `Util/`: Utility functions and helpers
- `include/`: Header files for analysis libraries
- `tools/`: Command-line tools and analysis passes
  - `Sparrow/`: Call graph construction
  - `WPA/`: Whole program points-to analysis in SVF
  - `SABER/`: Value-flow based Memory bug detection in SVF
  - `taint-check/`: Taint analysis
  - `global-pts/`: Global points-to analysis
  - `pts-dump/`: Points-to information dump
  - `pts-inst/`: Points-to instrumentation (for dynamic alias analysis)
  - `pts-verify/`: Points-to verification
  - `vkcfa-taint/`: Value and context flow-aware taint analysis
  - `table/` and `table-check/`: Analysis table generation and verification
  - `dot-du-module/`: Dot graph generator for def-use chains
  - `dot-pointer-cfg/`: Dot graph generator for pointer CFGs
- `config/`: Configuration files for various analyses
- `benchmark/`: Benchmark programs for testing

## Related

- https://github.com/vmpaisante/sraa, Pointer Disambiguation via Strict Inequalities, CGO'17
- https://github.com/vmpaisante/obaa
- [DG](https://github.com/mchalupa/dg) - Dependence Graph for analysis of LLVM bitcode
- [AserPTA](https://github.com/PeimingLiu/AserPTA) - Andersen's points-to analysis
- [TPA](https://github.com/grievejia/tpa) - A flow-sensitive, context-sensitive pointer analysis
- [Andersen](https://github.com/grievejia/andersen) - Andersen's points-to analysis
- [SUTURE](https://github.com/seclab-ucr/SUTURE) - Static analysis for security
- [LLVM Opt Benchmark](https://github.com/dtcxzyw/llvm-opt-benchmark) - LLVM optimization benchmarks
- [EOS](https://github.com/gpoesia/eos) - ?
- https://github.com/gleisonsdm/DawnCC-Compiler
- https://github.com/hotpeperoncino/sfs