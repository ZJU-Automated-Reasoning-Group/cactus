Saber
==============

Cactus provides memory analysis capabilities through the SABER tool, which can detect various memory-related bugs.

SABER
----

SABER is a static bug checker for memory issues like leaks, use-after-free, and double-free problems. It leverages the SVF framework to perform value-flow tracking and identify memory management errors.

Features:

* **Memory Leak Detection**: Identifies allocated memory that is never freed.
* **Use-After-Free Detection**: Finds uses of memory after it has been freed.
* **Double-Free Detection**: Detects when the same memory is freed multiple times.
* **Null Pointer Dereference**: Identifies potential null pointer dereferences.
* **Buffer Overflow Analysis**: Analyzes potential buffer overflows.

The analysis works by tracking memory allocations and frees through the program and identifying inconsistent operations.

Using SABER
---------

SABER can be used through the command line:

.. code-block:: bash

    # Detect memory leaks
    ./saber your_program.bc -leak
    
    # Detect use-after-free
    ./saber your_program.bc -uaf
    
    # Detect double-free
    ./saber your_program.bc -dfree
    
    # Enable all detectors
    ./saber your_program.bc -leak -uaf -dfree
    
    # Print statistics
    ./saber your_program.bc -leak -stat

