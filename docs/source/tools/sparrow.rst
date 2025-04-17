Call Graph Construction
=====================

Cactus provides Sparrow, a powerful call graph construction tool that focuses on resolving indirect calls through function pointer analysis.

Sparrow
------

Sparrow constructs a precise call graph by focusing on resolving indirect function calls. It uses pointer analysis to determine the potential targets of function pointers.

Indirect Call Resolution
----------------------

One of the key challenges in static analysis is resolving indirect calls, which include:

* Function pointer calls in C (``(*fptr)(args)``)
* Virtual method calls in C++ (``obj->method(args)``)
* Function objects and lambdas

Sparrow uses type-based analysis and points-to analysis to resolve these indirect calls by determining the set of functions that each function pointer may point to.

Call Graph Output
--------------

Sparrow can generate several outputs:

1. **Transformed Bitcode**: The tool can emit transformed LLVM bitcode with indirect calls replaced by direct call instructions (with the ``-dump-bc`` option).

2. **Call Target Report**: The tool generates a report file (``indirect-call-targets.txt``) containing information about indirect call targets (with the ``-dump-report`` option).

3. **DOT Graph Visualization**: The tool can generate a DOT file of the call graph for visualization (with the ``-dot-callgraph`` option).

Report Format
-----------

The ``indirect-call-targets.txt`` report includes:

* File path of the call site
* Function name containing the call site
* Line number of the call site
* Number of target callees
* Function names of the target callees

Example report entry:

.. code-block:: none

    File: /path/to/source.c
    Function: main
    Line: 42
    Num Targets: 2
    Targets: [foo, bar]

Using the Call Graph
------------------

The call graph can be used programmatically in your own analyses:

.. code-block:: cpp

    // TBD
    