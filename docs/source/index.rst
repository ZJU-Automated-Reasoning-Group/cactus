Welcome to Cactus's Documentation!
================================

Introduction
===========

Cactus is a powerful static analysis framework designed to work with LLVM bitcode, providing advanced capabilities for call graph construction, pointer analysis, and taint analysis. 

The current version has been tested with LLVM 3.6.2 and requires a C++14 compliant compiler.

Major Components
===============

Lightweight Alias/Pointer Analysis
-------------

* **SRAA**: Strict Relation Alias Analysis that identifies non-aliasing pointers by analyzing their strict mathematical relationships
* **OBAA**: Offset-Based Alias Analysis that leverages offset information to determine alias relationships
* **Canary**: A lightweight alias analysis
* **SVF-based analyses**: Andersen's, Steensgaard's, and other implementations from the SVF framework

Call Graph Construction
----------------------

* **Sparrow**: Constructs a precise call graph by resolving indirect calls through function pointer analysis

Flow and Context-sensitive Pointer Analysis
--------------

* **Flow-sensitive analysis**: Tracks pointer values with respect to control flow
* **Context-sensitive analysis**: Differentiates pointer values between different calling contexts

Taint Analysis
------------

* **taint-check**: Tracks the flow of potentially malicious or sensitive data throughout the program
* **vkcfa-taint**: Value and context flow-aware taint analysis

.. toctree::
   :maxdepth: 2
   :caption: Contents:
   
   getting_started/index
   components/index
   tools/index
   dev_guide/index
   examples/index 