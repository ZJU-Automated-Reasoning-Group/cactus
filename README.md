# Sparrow



## Build

```
cd sparrow
mkdir build && cd build
cmake ../ -DLLVM_BUILD_PATH=[LLVM3.6.2 Build Path]
make -jN
```

## Usage


After successful build, the executable is in the build/bin folder. 
For source code, you have to compile it to LLVM bitcode first. 
For binary code, you have to first use plankton-dasm to lift it to LLVM bitcode.

```
./sparrow  your_project.bc
```


## Functionality
Source code for call graph construction framework written in C++.
It can accept a LLVM BC, producing either a transformed BC (concerting indirect calls to direct calls)
or dump the indirect-call targets in .txt form.

You can dump the transformed BC without indirect calls in the below way. The new BC can only be used for static analysis.

```
./sparrow  your_project.bc -dump-bc
```

You can dump the indirect-call results in the below way.

```
./sparrow  your_project.bc -dump-report 
```

The report is an indirect-call-targets.txt file with five kinds of information for each indirect call.
* The file name of the call site
* The function name of the function where the call site is
* The line number of the call site
* The number of the callees
* The function name of the callees

You can read the indirect-call-targets.txt in the below way

```
 if (filePath.empty()) {
        filePath = "indirect-call-targets.txt"; // read from file.txt in current directory
    }

    std::ifstream file(filePath);
    if (file.is_open()) {
        bool done = false;
        while (!done) {

            std::string file_path; // the file path
            std::getline(file, file_path);

            std::string caller; // the function where the call site is
            std::getline(file, caller);

            std::string icall_line_str; 
            std::getline(file, icall_line_str);
            std::string callee_size_str;
            std::getline(file, callee_size_str);
            int icall_line; // the line of the call site
            int callee_size; // the callee number 

            try {
                icall_line = std::stoi(icall_line_str);
                callee_size= std::stoi(callee_size_str);
            } catch (const std::invalid_argument& e) {
                std::cout << "Invalid argument: " << e.what() << std::endl;
                return;
            } catch (const std::out_of_range& e) {
                std::cout << "Out of range: " << e.what() << std::endl;
                return;
            }

            
            std::vector<string> callees_names;
            for (int i = 0; i < callee_size; i++) {
                std::string line; // the callee name
                std::getline(file, line); 
                callees_names.push_back(line);
            }
            
            // Check if we have reached the end of the file
            if (file.peek() == EOF) {
                done = true;
            }
        }

    } else {
        std::cerr << "Unable to open file: " << filePath << std::endl;
    }
```