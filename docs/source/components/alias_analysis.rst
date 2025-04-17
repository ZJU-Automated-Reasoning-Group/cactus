Alias Analysis
=============

Cactus includes a comprehensive collection of alias analysis implementations that provide different precision/performance trade-offs for identifying when two pointers may reference the same memory location.

- SRAA (Strict Relation Alias Analysis)
- OBAA (Offset-Based Alias Analysis)
- Canary (Dyck Language Reachability)
- XXXX (Flow-Sensitive and Context-Sensitive Pointer Analysis)


SRAA (Strict Relation Alias Analysis)
------------------------------------

SRAA identifies non-aliasing pointers by analyzing their strict mathematical relationships. It leverages mathematical properties to prove that certain pointers cannot alias.

Features:

* Uses range analysis to determine variable bounds
* Implements value-based Static Single Assignment (SSA) form
* Can prove non-aliasing through strict inequality relationships
* Helps eliminate false positives in other analyses

Implementation reference: https://github.com/vmpaisante/sraa, based on the paper "Pointer Disambiguation via Strict Inequalities" (CGO'17)

OBAA (Offset-Based Alias Analysis)
--------------------------------

OBAA leverages offset information to determine alias relationships between pointers. It tracks how far pointers are from their allocation base to disambiguate pointers to different fields of the same object.

Features:

* Field-sensitive analysis
* Tracks pointer offsets relative to their base objects
* Can distinguish pointers to different struct fields
* Complements other alias analyses by focusing on intra-object relationships

Implementation reference: https://github.com/vmpaisante/obaa

Canary (Dyck Language Reachability)
-----

Canary is a lightweight, Dyck language reachability-based alias analysis designed for efficient alias checking.

Features:

* Low overhead compared to more precise analyses
* Focuses on commonly exploited pointer patterns
* Designed specifically for security analysis applications
* Can be used as a pre-filter for more expensive analyses

Using Alias Analysis
------------------


Available alias results include:

* ``NoAlias``: The two pointers never reference the same memory location
* ``MayAlias``: The two pointers might reference the same memory location
* ``MustAlias``: The two pointers always reference the same memory location
* ``PartialAlias``: The two references overlap but do not start at the same location

Performance Considerations
------------------------


For best results, consider using a combination of alias analyses based on your specific use case. 