

#include "Util/DataStructure/worklist.h"

bool WLNode::prior_than(const WLNode *o_node) const {
  return priority > o_node->priority;
}

bool WLNode::prior_than(const WLNode &o_node) const {
  return priority > o_node.priority;
}

Worklist::Worklist() : multi_enq(false), keep_trace(false) {}

// -------------------Priority queue start------------------------
PQ_Worklist::PQ_Worklist(bool trace) : Worklist(), sorted(true), nodes(31) {
  keep_trace = trace;
  if (trace) {
    keep_trace = true;
    nodes_traced.resize(31);
  }
  init();
}

PQ_Worklist::PQ_Worklist(int n, bool trace)
    : Worklist(), sorted(true), nodes(n) {
  if (trace) {
    keep_trace = true;
    nodes_traced.resize(n);
  }
  init();
}

PQ_Worklist::~PQ_Worklist() {}

int PQ_Worklist::size() { return nodes.size(); }

bool PQ_Worklist::empty() { return nodes.size() <= 1; }

// Get a node from worklist
// Return null if the worklist is empty
WLNode *PQ_Worklist::front() {
  int qtail = nodes.size();
  WLNode *ret = nullptr;

  if (qtail > 1) {
    ret = nodes[1];
    WLNode *e = nodes[qtail--];
    heap_down(1, e);
    ret->wl_pos = WLNode::VISITED;
  }

  return ret;
}

// Insert a new node
bool PQ_Worklist::push_back(WLNode *e) {
  int k = e->wl_pos;
  int ret_status = false;

  if (k == WLNode::UNVISITED || (multi_enq && k == WLNode::VISITED)) {
    k = nodes.size();
    nodes.push_back(e);
    ret_status = true;
  } else if (!multi_enq) {
    return false;
  }

  if (keep_trace)
    nodes_traced.push_back(e);

  while (k > 1) {
    int kk = k / 2;
    if (nodes[kk]->prior_than(e))
      break;
    nodes[k] = nodes[kk];
    nodes[k]->wl_pos = k;
    k /= 2;
  }

  e->wl_pos = k;
  nodes[k] = e;
  sorted = false;
  return ret_status;
}

void PQ_Worklist::clear() {
  nodes.clear();
  if (keep_trace)
    nodes_traced.clear();
  init();
}

void PQ_Worklist::init() { nodes.push_back(nullptr); }

void PQ_Worklist::heap_down(int i_pos, WLNode *e) {
  int k = i_pos, kk;
  int qtail = nodes.size();

  while ((kk = k * 2) < qtail) {
    if ((kk + 1) < qtail && nodes[kk + 1]->prior_than(nodes[kk]))
      kk++;
    if (e->prior_than(nodes[kk]))
      break;
    nodes[k] = nodes[kk];
    nodes[k]->wl_pos = k;
    k = kk;
  }

  e->wl_pos = k;
  nodes[k] = e;
}

void PQ_Worklist::heapsort() {
  int qtail = nodes.size();

  if (qtail > 2) {
    // First generate a min-first sort
    for (int i = qtail - 1; i > 1; --i) {
      WLNode *e = nodes[i];
      nodes[i] = nodes[1];
      heap_down(1, e);
    }

    // Reverse to obtain the max-first sort
    nodes.reverse(1);
  }

  sorted = true;
}

// -------------------FIFO queue start------------------------
FIFO_Worklist::FIFO_Worklist(bool trace) : Worklist(), q_head(0) {
  keep_trace = trace;
  nodes_traced.resize(31);
}

FIFO_Worklist::~FIFO_Worklist() {}

int FIFO_Worklist::size() { return nodes_traced.size() - q_head; }

bool FIFO_Worklist::empty() { return q_head == nodes_traced.size(); }

// Get a node from worklist
// Return null if the worklist is empty
WLNode *FIFO_Worklist::front() {
  WLNode *ret = nullptr;

  if (q_head < nodes_traced.size()) {
    ret = nodes_traced[q_head];
    ret->wl_pos = WLNode::VISITED;
    q_head++;
  }

  return ret;
}

// Insert a new node
bool FIFO_Worklist::push_back(WLNode *e) {
  int k = e->wl_pos;
  int ret_status = false;

  if (k == WLNode::UNVISITED || (multi_enq && k == WLNode::VISITED)) {
    e->wl_pos = nodes_traced.size();
    nodes_traced.push_back(e);
    ret_status = true;
  }

  return ret_status;
}

void FIFO_Worklist::clear() {
  nodes_traced.clear();
  q_head = 0;
}