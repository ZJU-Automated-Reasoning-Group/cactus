# Sparrow

Sparrow is a powerful analysis tool designed to work with LLVM bitcode, providing advanced capabilities for call graph construction, pointer analysis, and taint analysis.

## Build Instructions

To build Sparrow, follow these steps:

```bash
cd sparrow
mkdir build && cd build
cmake ../ -DLLVM_BUILD_PATH=[LLVM3.6.2 Build Path]
make -jN
```

Replace `[LLVM3.6.2 Build Path]` with the path to your LLVM 3.6.2 build.

## Usage

After building, the executable is located in the `build/bin` folder. To use Sparrow, you need to compile your source code to LLVM bitcode first. For binary code, use `plankton-dasm` to lift it to LLVM bitcode.

Run Sparrow with:

```bash
./sparrow your_project.bc
```

## Core Functionalities

Sparrow provides three core analysis capabilities:

### 1. Call Graph Construction
- **Description**: Constructs a call graph by resolving indirect calls.
- **Components**:
  - `lib/IndirectCallResolver`: Core call graph construction
  - `tools/callgraph`: LLVM pass implementation

### 2. Advanced Pointer Analysis
- **Description**: Implements Andersen-style pointer analysis to track pointer relationships and aliasing.
- **Components**:
  - `lib/PointerAnalysis`: Pointer analysis engine
  - `tools/analysis-passes/PointerAnalysisPass.cpp`: LLVM integration
  - `include/Analysis/PointsTo.h`: Analysis interface

Use with:

```bash
./sparrow your_project.bc -pointer-analysis -dump-report
```

### 3. Taint Analysis Framework
- **Description**: Tracks the flow of potentially malicious or sensitive data.
- **Components**:
  - `lib/TaintAnalysis`: Taint propagation engine
  - `tools/analysis-passes/TaintAnalysisPass.cpp`: Data flow tracking
  - `include/Analysis/Taint.h`: Taint configuration API

Enable with:

```bash
./sparrow your_project.bc -taint-analysis -taint-config=config.yaml
```

## Additional Commands

- **Dump Transformed BC**: Remove indirect calls for static analysis.
  ```bash
  ./sparrow your_project.bc -dump-bc
  ```

- **Dump Indirect Call Results**: Outputs indirect call targets to `indirect-call-targets.txt`.
  ```bash
  ./sparrow your_project.bc -dump-report
  ```

The report includes:
- File name of the call site
- Function name where the call site is
- Line number of the call site
- Number of callees
- Function names of the callees

## Reading the Report

To read the `indirect-call-targets.txt` file, use the following code snippet:

```cpp
if (filePath.empty()) {
    filePath = "indirect-call-targets.txt"; // Default file path
}

std::ifstream file(filePath);
if (file.is_open()) {
    bool done = false;
    while (!done) {
        std::string file_path;
        std::getline(file, file_path);

        std::string caller;
        std::getline(file, caller);

        std::string icall_line_str;
        std::getline(file, icall_line_str);
        std::string callee_size_str;
        std::getline(file, callee_size_str);
        int icall_line;
        int callee_size;

        try {
            icall_line = std::stoi(icall_line_str);
            callee_size = std::stoi(callee_size_str);
        } catch (const std::invalid_argument& e) {
            std::cout << "Invalid argument: " << e.what() << std::endl;
            return;
        } catch (const std::out_of_range& e) {
            std::cout << "Out of range: " << e.what() << std::endl;
            return;
        }

        std::vector<std::string> callees_names;
        for (int i = 0; i < callee_size; i++) {
            std::string line;
            std::getline(file, line);
            callees_names.push_back(line);
        }

        if (file.peek() == EOF) {
            done = true;
        }
    }
} else {
    std::cerr << "Unable to open file: " << filePath << std::endl;
}
```
