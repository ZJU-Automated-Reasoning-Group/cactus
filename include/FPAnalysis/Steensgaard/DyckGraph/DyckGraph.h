#pragma once

#include "DyckVertex.h"
#include <unordered_map>
#include <stack>

using namespace std;

namespace Steensgaard{

/// This class models a dyck-cfl language as a graph, which does not contain the barred edges.
/// See details in http://dl.acm.org/citation.cfm?id=2491956.2462159&coll=DL&dl=ACM&CFID=379446910&CFTOKEN=65130716 .
    class DyckGraph {
    private:
        set<DyckVertex*> vertices;

        std::set<void*> fp_realted_vals; // we store the interesting values

        unordered_map<void *, DyckVertex*> val_ver_map;
    public:
        DyckGraph() {
        }
        ~DyckGraph() {
            for (auto& v : vertices) {
                delete v;
            }
        }

        /// The number of vertices in the graph.
        unsigned int numVertices();

        /// The number of equivalent sets.
        /// Please use it after you call void qirunAlgorithm().
        unsigned int numEquivalentClasses();

        /// Get the set of vertices in the graph.
        set<DyckVertex*>& getVertices();

        /// You are not recommended to use the function when the graph is big,
        /// because it is time-consuming.
        void printAsDot(const char * filename) const;

        /// Combine x's rep and y's rep.
        DyckVertex* combine(DyckVertex* x, DyckVertex* y);

        /// if value is NULL, a new vertex will be always returned with false.
        /// if value's vertex has been initialized, it will be returned with true;
        /// otherwise, it will be initialized and returned with false;
        /// If a new vertex is initialized, it will be added into the graph.
        pair<DyckVertex*, bool> retrieveDyckVertex(void * value, const char* name = NULL);

        DyckVertex* findDyckVertex(void* value);

        /// The algorithm proposed by Qirun Zhang.
        /// Find the paper here: http://dl.acm.org/citation.cfm?id=2491956.2462159&coll=DL&dl=ACM&CFID=379446910&CFTOKEN=65130716 .
        /// Note that if there are two edges with the same label: a->b and a->c, b and c will be put into the same equivelant class.
        /// If the function does nothing, return true, otherwise return false.
        bool qirunAlgorithm();

        /// validation
        void validation(const char*, int);

        std::set<void*> getFPResultValueSet();

        void updateFPResultValueSet(void* val){
            this->fp_realted_vals.insert(val);
        }

    private:
        void removeFromWorkList(multimap<DyckVertex*, void*>& list, DyckVertex* v, void* l);

        bool containsInWorkList(multimap<DyckVertex*, void*>& list, DyckVertex* v, void* l);
    };
}


