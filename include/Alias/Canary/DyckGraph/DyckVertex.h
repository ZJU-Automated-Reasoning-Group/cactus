#pragma once

#include <map>
#include <set>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

namespace Canary{

    class DyckGraph;
/// This class models the vertex in DyckGraph.

    class DyckVertex {
    private:
        static int global_indx;
        int index;
        const char * name;

        set<void*> in_lables;
        set<void*> out_lables;

        map<void*, set<DyckVertex*>> in_vers;
        map<void*, set<DyckVertex*>> out_vers;

        /// only store non-null value
        set<void*> equivclass;

        /// Default constructor is not visible.
        /// please use DyckGraph::retrieveDyckVertex for initialization
        DyckVertex();

        /// The constructor is not visible. The first argument is the pointer of the value that you want to encapsulate.
        /// The second argument is the name of the vertex, which will be used in void DyckGraph::printAsDot() function.
        /// You are not recommended to assign names to vertices when you need not to print the graph,
        /// because it may be time-consuming for you to construct names for vertices.
        /// please use DyckGraph::retrieveDyckVertex for initialization.
        DyckVertex(void * v, const char* itsname = NULL);

    public:
        friend class DyckGraph;

    public:
        ~DyckVertex();

        /// Get its index
        /// The index of the first vertex you create is 0, the second one is 1, ...
        int getIndex();

        /// Get its name
        const char * getName();

        /// Get the source vertices corresponding the label
        set<DyckVertex*>* getInVertices(void * label);

        /// Get the target vertices corresponding the label
        set<DyckVertex*>* getOutVertices(void * label);

        /// Get the number of vertices that are the targets of this vertex, and have the edge label: label.
        unsigned int outNumVertices(void* label);

        /// Get the number of vertices that are the sources of this vertex, and have the edge label: label.
        unsigned int inNumVertices(void* label);

        /// Total degree of the vertex
        unsigned int degree();

        /// Get all the labels in the edges that point to the vertex's targets.
        set<void*>& getOutLabels();

        /// Get all the labels in the edges that point to the vertex.
        set<void*>& getInLabels();

        /// Get all the vertex's targets.
        /// The return value is a map which maps labels to a set of vertices.
        map<void*, set<DyckVertex*>>& getOutVertices();

        /// Get all the vertex's sources.
        /// The return value is a map which maps labels to a set of vertices.
        map<void*, set<DyckVertex*>>& getInVertices();

        /// Add a target with a label. Meanwhile, this vertex will be a source of ver.
        void addTarget(DyckVertex* ver, void* label);

        /// Remove a target. Meanwhile, this vertex will be removed from ver's sources
        void removeTarget(DyckVertex* ver, void* label);

        /// Return true if the vertex contains a target ver, and the edge label is "label"
        bool containsTarget(DyckVertex* ver, void* label);

        /// For qirun's algorithm DyckGraph::qirunAlgorithm().
        /// The representatives of all the vertices in the equivalent set of this vertex
        /// will be set to be rep.
        void mvEquivalentSetTo(DyckVertex* rep);

        /// Get the equivalent set of non-null value.
        /// Use it after you call DyckGraph::qirunAlgorithm().
        set<void*>* getEquivalentSet();

    private:
        void addSource(DyckVertex* ver, void* label);
        void removeSource(DyckVertex* ver, void* label);
    };


}

