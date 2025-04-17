Configuration Files
=================

Cactus uses several configuration files to customize the behavior of its analyses. These files are located in the ``config/`` directory.

ptr.config
---------

The ``ptr.config`` file specifies pointer behavior of library functions, which is essential for accurate pointer analysis.

Format:

.. code-block:: none

    FUNCTION_NAME BEHAVIOR [PARAMETERS]

Common behaviors:

* ``ALLOC``: The function allocates memory (e.g., ``malloc``, ``calloc``)
* ``COPY``: The function copies data from one location to another (e.g., ``strcpy``, ``memcpy``)
* ``FREE``: The function frees memory (e.g., ``free``)
* ``IGNORE``: The function can be safely ignored in pointer analysis

Examples:

.. code-block:: none

    # Memory allocation functions
    malloc ALLOC
    calloc ALLOC
    realloc ALLOC
    
    # Memory copy functions
    strcpy COPY Arg0 V Arg1 V
    memcpy COPY Arg0 V Arg1 V
    
    # Memory free functions
    free FREE Arg0 V
    
    # Functions to ignore
    chmod IGNORE

taint.config
----------

The ``taint.config`` file defines taint sources, sinks, and propagation rules for taint analysis.

Format:

.. code-block:: none

    TYPE FUNCTION_NAME [PARAMETERS]

Types:

* ``SOURCE``: Defines a function that introduces tainted data
* ``SINK``: Defines a function where tainted data could cause problems
* ``PIPE``: Defines how taint propagates through a function
* ``IGNORE``: Specifies functions to ignore in taint analysis
* ``SANITIZE``: Defines functions that sanitize tainted data

Examples:

.. code-block:: none

    # Taint sources (user input)
    SOURCE fgetc Ret V T
    SOURCE read Arg1 D T
    SOURCE fgets Arg0 D T
    
    # Taint sinks (sensitive operations)
    SINK system Arg0 D
    SINK execl Arg0 D
    SINK strcpy Arg0 D
    
    # Taint propagation
    PIPE memcpy Arg0 R Arg1 R
    PIPE strcat Arg0 R Arg1 R
    
    # Functions to ignore
    IGNORE free
    IGNORE puts
    
    # Sanitization functions
    SANITIZE isalpha Arg0 T Ret V

modref.config
-----------

The ``modref.config`` file specifies memory effects of library functions for modify/reference analysis.

Format:

.. code-block:: none

    FUNCTION_NAME MOD_EFFECT REF_EFFECT

Effects:

* ``MOD_ARG``: The function modifies its arguments
* ``MOD_GLOBAL``: The function modifies global memory
* ``REF_ARG``: The function references its arguments
* ``REF_GLOBAL``: The function references global memory
* ``NONE``: No effect

Examples:

.. code-block:: none

    # Memory functions
    malloc REF_ARG NONE
    strcpy MOD_ARG REF_ARG
    free MOD_ARG NONE
    
    # I/O functions
    printf REF_ARG REF_GLOBAL
    fgets MOD_ARG REF_ARG
    
    # System functions
    system REF_ARG MOD_GLOBAL
    getenv REF_ARG REF_GLOBAL

Customizing Configurations
------------------------

You can customize these configuration files for your specific needs:

1. Create a copy of the default configuration file
2. Modify it with your custom function behaviors
3. Pass it to the analysis tool using the appropriate option:

.. code-block:: bash

    ./taint-check your_program.bc -taint-config my-custom-taint.config

Best Practices
------------

* Start with the default configurations, which cover most common library functions
* Add configurations for custom or domain-specific functions
* Be conservative: when in doubt, assume more effects rather than fewer
* Document the reasoning behind custom configurations
* Validate configurations with test cases when possible 