Taint Analysis
=============

Cactus provides a powerful taint analysis framework that tracks the flow of potentially sensitive or malicious data throughout a program.

Overview
-------

Taint analysis is a security-focused technique that tracks how "tainted" data (such as user input) flows through a program. It helps identify potential security vulnerabilities by determining if tainted data can reach sensitive program points (such as system calls or memory operations) without proper sanitization.

Taint Tracking
------------

The taint analysis in Cactus tracks data flow in the following ways:

* **Direct Value Flow**: Tracks data flow through direct assignments.
* **Pointer-based Flow**: Uses pointer analysis to track flow through memory.
* **Interprocedural Flow**: Tracks taint across function boundaries.
* **Field-sensitive Flow**: Distinguishes between different fields of structures.
* **Context-sensitive Flow**: Differentiates between calling contexts for higher precision.

Taint Sources, Sinks, and Sanitizers
----------------------------------

The taint analysis framework defines:

* **Sources**: Program points where tainted data enters (e.g., user input functions like ``fgets``, ``read``).
* **Sinks**: Program points where tainted data could cause problems (e.g., memory operations, system calls).
* **Sanitizers**: Functions that clean or validate tainted data. (TBD)
* **Propagators**: Functions that transfer taint from one location to another.

These are defined in the ``taint.config`` file.

Value and Context Flow-Aware Taint Analysis
-----------------------------------------

The ``vkcfa-taint`` tool implements a value and context flow-aware taint analysis that offers higher precision than traditional taint analyses:

* **Value-aware**: Tracks the specific values that can flow to each program point.
* **Context-aware**: Distinguishes taint based on the calling context.

Using Taint Analysis
------------------

Taint analysis can be used through the ``taint-check`` tool:

.. code-block:: bash

    ./taint-check your_program.bc -taint-config ../config/taint.config

Programmatic usage:

.. code-block:: cpp

    TBD

Taint Configuration
-----------------

Taint analysis behavior is customized through the ``taint.config`` file:

.. code-block:: none

    SOURCE fgetc Ret V T          # Return value is a taint source
    SINK printf Arg0 D            # First argument is a taint sink
    PIPE memcpy Arg0 R Arg1 R     # Taint propagates from Arg1 to Arg0
    IGNORE free                   # Function call can be ignored in taint analysis
    SANITIZE isalpha Arg0 T Ret V # isalpha sanitizes its input （TBD)

Configuration format:

* ``SOURCE``: Defines a taint source
* ``SINK``: Defines a taint sink
* ``PIPE``: Defines taint propagation
* ``IGNORE``: Specifies functions to ignore
* ``SANITIZE``: Defines a taint sanitizer （TBD)

Security Applications
-------------------

Taint analysis can detect various security vulnerabilities:

* **Buffer Overflows**: Detecting when tainted data can influence buffer sizes
* **SQL Injection**: Tracking user input to database query functions
* **Command Injection**: Tracking user input to system command functions
* **Format String Vulnerabilities**: Detecting tainted format strings in printf-like functions
* **Cross-Site Scripting (XSS)**: Tracking user input to output functions in web applications
