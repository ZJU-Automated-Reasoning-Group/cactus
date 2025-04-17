Basic Usage
==========

This guide provides an overview of how to use Cactus tools for analyzing LLVM bitcode files.

Generating LLVM Bitcode
----------------------

Before using Cactus, you need to compile your source code to LLVM bitcode:

.. code-block:: bash

    # For C code
    clang -emit-llvm -c -g -O0 your_source.c -o your_source.bc
    
    # For C++ code
    clang++ -emit-llvm -c -g -O0 your_source.cpp -o your_source.bc
    
    # For multiple files, compile them separately and then link
    llvm-link file1.bc file2.bc -o linked.bc

Common Command-Line Options
-------------------------

Most Cactus tools support the following common options:

.. code-block:: bash

    -ptr-config <file>      # Specify a custom ptr.config file
    -taint-config <file>    # Specify a custom taint.config file 
    -modref-config <file>   # Specify a custom modref.config file
    -dot-callgraph          # Generate a DOT file of the call graph
    -dump-ir                # Dump the transformed IR
    -verbose                # Enable verbose output
    -stats                  # Print analysis statistics

Each tool may have additional specific options. Use ``-help`` with any tool to see all available options.

Example Workflows
---------------

sparrow: Call Graph Construction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    ./sparrow your_project.bc -dump-bc -dump-report
    
This will:
1. Analyze your bitcode file
2. Dump the transformed bitcode with resolved indirect calls
3. Generate an ``indirect-call-targets.txt`` file with call target information

wpa: Whole Program Pointer Analysis
^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    ./wpa your_project.bc -ander -stat
    
This will:
1. Run Andersen's pointer analysis
2. Print statistics about the analysis

taint_check: Taint Analysis
^^^^^^^^^^^^

.. code-block:: bash

    ./taint-check your_project.bc -taint-config ../config/taint.config
    
This will:
1. Run taint analysis with the specified configuration
2. Report any taint violations

saber: Value-Flow based Memory Bug Detection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    ./saber your_project.bc -leak -uaf -stat
    
This will:
1. Analyze your bitcode for memory leaks and use-after-free bugs
2. Print statistics about the analysis

Visualizing Results
--------------------

Many Cactus tools can generate DOT graph files for visualization:

.. code-block:: bash

    ./wpa your_project.bc -ander -dump-callgraph
    dot -Tpdf callgraph_wpa.dot -o callgraph.pdf
    
    # For pointer CFG visualization
    ./dot-pointer-cfg your_project.bc -function=main
    dot -Tpdf pointer_cfg_main.dot -o pointer_cfg.pdf 