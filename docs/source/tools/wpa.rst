WPA (Whole Program Analysis)
=========================

WPA is from the the SVF (Static Value-Flow) framework. It provides various pointer analysis algorithms with different precision and performance trade-offs.

Overview
-------

WPA supports several pointer analysis algorithms:

* **Andersen's Analysis** (``-ander``): An inclusion-based pointer analysis that models pointer assignments as subset constraints
* **Steensgaard's Analysis** (``-steens``): A unification-based analysis that provides faster but less precise results
* **Flow-Sensitive Analysis** (``-fspta``): Tracks pointer values with respect to program control flow

Basic Usage
------------

To run Andersen's pointer analysis:

.. code-block:: bash

    ./wpa your_program.bc -ander

To run flow-sensitive pointer analysis:

.. code-block:: bash

    ./wpa your_program.bc -fspta

Output Options:

* ``-dump-callgraph``: Generate call graph dot file
* ``-dump-pag``: Generate pointer assignment graph dot file
* ``-dump-consG``: Generate constraint graph dot file
* ``-dump-svfg``: Generate sparse value-flow graph dot file
* ``-print-pts``: Print points-to information

Customization:

* ``-ptr-config <file>``: Specify custom pointer behavior configuration file

Example Workflows
---------------

Basic Analysis with Statistics
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    ./wpa your_program.bc -ander -stat

This runs Andersen's pointer analysis and prints statistics including:
- Number of identified pointers
- Size of points-to sets
- Analysis time


Visualization
^^^^^^^^^^^

.. code-block:: bash

    ./wpa your_program.bc -ander -dump-callgraph -dump-pag
    dot -Tpdf pag_ander.dot -o pointer_graph.pdf
    dot -Tpdf callgraph_ander.dot -o call_graph.pdf

This generates visualizations of the pointer assignment graph and call graph.

Performance Considerations
------------------------

Analysis precision and performance vary significantly:

* **Steensgaard's Analysis**: Fastest but least precise
* **Andersen's Analysis**: Good balance of precision and performance
* **Flow-Sensitive Analysis**: More precise but slower

Use the ``-stat`` option to measure performance and precision metrics for your specific program.



WPA leverages the SVF framework, which provides:

* Pointer Assignment Graph (PAG) construction
* Value-flow tracking
* Memory SSA representation
* Call graph construction
* Points-to propagation algorithms


Programmatic Usage
----------------

WPA can also be used programmatically in your own analyses:

.. code-block:: cpp

    // TBD

See the SVF documentation for more details on the API.
