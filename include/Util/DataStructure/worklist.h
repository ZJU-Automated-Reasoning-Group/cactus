/*
 * Various efficient implementations of worklist.
 * Majorly used by graph traversal algorithms.
 *
 *  Created on: 10 Feb, 2015
 *      Author: xiao
 */

#ifndef UTILS_ADT_WORKLIST_H
#define UTILS_ADT_WORKLIST_H

#include "Util/DataStructure/kvec.h"
#include <climits>

class PQ_Worklist;
class FIFO_Worklist;

// The meta class of the node that can work with the worklist
// One issue for current design is that a node at one time can only be put into
// one worklist.
class WLNode {
public:
  enum VisFlag { UNVISITED = -1, VISITED = INT_MAX };

  WLNode() : priority(0), wl_pos(UNVISITED) {}
  WLNode(int pri) : priority(pri), wl_pos(UNVISITED) {}
  virtual ~WLNode() {}

public:
  void set_priority(int prio) { priority = prio; }
  int get_priority() { return priority; }

  /*
   * Using wl_pos to decide the visiting status of a node costs only O(1) time.
   * However, it also restricts a node to reside at most in one queue at a time.
   */
  void reset_pos() { wl_pos = UNVISITED; }
  bool unvisited() { return wl_pos == UNVISITED; }
  bool inQ() { return wl_pos > UNVISITED && wl_pos < VISITED; }
  bool visited() { return wl_pos == VISITED; }

  // Cast this function to a desired derived type
  template <typename T> T *getAs() {
    // We don't check the type compatibility
    return static_cast<T *>(this);
  }

  virtual bool prior_than(const WLNode *) const;
  virtual bool prior_than(const WLNode &) const;

protected:
  // Used by worklist node selection
  // priority : if a prority based worklist is used, nodes selected based on
  // this order wl_pos: the position of this node in the worklist
  int priority;
  int wl_pos;

  friend class PQ_Worklist;
  friend class FIFO_Worklist;
};

// With this wrapper class, all types of objects can work with our own worklist
template <typename T> class WLNode_Wrapper : public WLNode {
public:
  WLNode_Wrapper(T &d) : WLNode(), data(d) {}
  WLNode_Wrapper(T &d, int pri) : WLNode(pri), data(d) {}
  void set_raw_data(T &d) { data = d; }
  T &get_raw_data() { return data; }

private:
  T data;
};

/*
 * Super class of all kinds of worklist.
 * The worklist can keep a trace of its enqueued elements and iterate them
 * later.
 */
class Worklist {
public:
  typedef kvec<WLNode *>::iterator WLIter;

  // ******** Temporarily comment following implementation of abstract base
  // class iterator
  // ******** Will re-enable whenever be useful
  // The interface for implementing real operations of iterator
  //	class WLIterImpl
  //	{
  //	public:
  //		virtual void increment() {};
  //		virtual WLNode* dereference() { return nullptr; }
  //		virtual bool comp_not_equal(WLIterImpl*) { return true; }
  //	};
  //
  //	// The default iterator implementation
  //	// It is a simple wrapper for the kvec iterator
  //	class WLIterDefaultImpl
  //			: public WLIterImpl
  //	{
  //	private:
  //		kvec_iter __it;
  //
  //	public:
  //		WLIterDefaultImpl(kvec_iter& it)
  //				: __it(it) { }
  //
  //		void increment() {
  //			++__it;
  //		}
  //
  //		WLNode* dereference() {
  //			return *__it;
  //		}
  //
  //		bool comp_not_equal(WLIterImpl* other) {
  //			WLIterDefaultImpl* other_real =
  // static_cast<WLIterDefaultImpl*>(other); 			return __it !=
  // other_real->__it;
  //		}
  //	};
  //
  //	// The base class for iterators over different implementations of
  // Worklist. 	class WLIter
  //	{
  //	private:
  //		WLIterImpl* iter_impl;
  //
  //	public:
  //		WLIter(WLIterImpl* impl) :
  //			iter_impl(impl) { }
  //
  //		// Move constructor, to simultate unique_ptr
  //		WLIter(WLIter& other) {
  //			iter_impl = other.iter_impl;
  //			other.iter_impl = nullptr;
  //		}
  //
  //		~WLIter() {
  //			if (iter_impl)
  //				delete iter_impl;
  //		}
  //
  //		WLIter& operator++() {
  //			iter_impl->increment();
  //			return *this;
  //		}
  //
  //		WLNode* operator*() {
  //			return iter_impl->dereference();
  //		}
  //
  //		bool operator!=(WLIter& other) {
  //			return iter_impl->comp_not_equal(other.iter_impl);
  //		}
  //	};

public:
  Worklist();
  virtual ~Worklist(){};

  // We allow enabling/disabling multiple enqueues in the use of the worklist
  void set_multi_enq(bool c) { multi_enq = c; }

  bool is_traced() { return keep_trace; }

public:
  virtual int size() = 0;
  virtual bool empty() = 0;
  // Access to and pop up the first node in worklist
  virtual WLNode *front() = 0;
  // Return true iff the given node is currently not in worklist
  virtual bool push_back(WLNode *) = 0;
  // Clear the worklist and its trace
  virtual void clear() = 0;

public:
  /*
   * Concrete implementations of the worklist should provide the way to iterate
   * their elements.
   */
  virtual WLIter begin() = 0;
  virtual WLIter end() = 0;

  // Interfaces to iterate the history of the worklist
  WLIter history_begin() {
    if (!keep_trace)
      return end();
    return nodes_traced.begin();
  }

  WLIter history_end() { return nodes_traced.end(); }

protected:
  bool multi_enq;
  bool keep_trace;
  kvec<WLNode *> nodes_traced;
};

// A heap-based priority queue implementation
class PQ_Worklist : public Worklist {
public:
  PQ_Worklist(bool trace = false);
  PQ_Worklist(int n, bool trace = false);
  virtual ~PQ_Worklist();

public:
  int size();
  bool empty();
  WLNode *front();
  bool push_back(WLNode *);
  void clear();

public:
  //  class PQIterImpl : public WLIterImpl {
  //  private:
  //	  PQ_Worklist* __pq;
  //
  //  public:
  //	  PQIterImpl(PQ_Worklist* pq) : __pq(pq) { }
  //
  //	  WLNode* dereference() {
  //		  return __pq->front();
  //	  }
  //
  //	  bool comp_not_equal(WLIterImpl* other) {
  //		  PQIterImpl* other_real = static_cast<PQIterImpl*>(other);
  //	  }
  //  };

  /*
   * Iterating over a priority queue.
   * We first run a heapsort to sort the array, then the iteration is a simple
   * sequential walk on the array.
   */
  WLIter begin() {
    if (!sorted)
      heapsort();
    return nodes.begin() + 1;
  }

  WLIter end() { return nodes.end(); }

private:
  // Initialize the heap
  void init();
  // Heaplify from the \p i_pos position with the element \p e
  void heap_down(int i_pos, WLNode *e);
  // An in-situ heap sort
  // A sorted array is also a valid heap
  void heapsort();

private:
  bool sorted;
  kvec<WLNode *> nodes;
};

// A FIFO queue, just a wrapper of STL queue
class FIFO_Worklist : public Worklist {
public:
  FIFO_Worklist(bool trace = false);
  virtual ~FIFO_Worklist();

public:
  int size();
  bool empty();
  WLNode *front();
  bool push_back(WLNode *);
  void clear();

  virtual WLIter begin() { return nodes_traced.begin() + q_head; }

  virtual WLIter end() { return nodes_traced.end(); }

private:
  int q_head;
};

#endif /* UTILS_ADT_WORKLIST_H */
