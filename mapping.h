#ifndef MAPPING_H
#define MAPPING_H

#include <vector>
#include <map>
#include <util.h>

#ifndef LED_COUNT
#error "mapping.h needs LED_COUNT defined"
#endif

#if LED_COUNT <= 0xFF
typedef uint8_t PixelIndex;
#elif LED_COUNT <= 0xFFFF
typedef uint16_t PixelIndex;
#else
typedef uint32_t PixelIndex;
#endif

using EdgeTypes = uint8_t;

// TODO: should these just be std::tuple instead of EdgeTypes unions?
typedef union {
  struct {
    EdgeTypes first;
    EdgeTypes second;
  } edgeTypes;
  uint16_t pair;
} EdgeTypesPair;

typedef union _EdgeTypesQuad {
  struct {
    EdgeTypes first;
    EdgeTypes second;
    EdgeTypes third;
    EdgeTypes fourth;
  } edgeTypes;
  uint32_t quad;
  _EdgeTypesQuad() { quad=0; }
  _EdgeTypesQuad(EdgeTypes type) { quad=0; this->edgeTypes.first = type; }
} EdgeTypesQuad;


struct Edge {
  PixelIndex from, to;
  EdgeTypes types;
  enum : uint8_t {
    none  = 0,
    all   = 0xFF,
  };
  // used to navigate pixel intersections with multiple edges with the same edge type. 
  // If A->B->C but G->B->H also, A->C and G->H can be continueTo.
  bool continueTo = false;
  
  Edge(PixelIndex from, PixelIndex to, EdgeTypes types, bool continueTo=false) : from(from), to(to), types(types), continueTo(continueTo) {};

  Edge transpose(std::map<uint8_t,uint8_t> &typesMap) {
    EdgeTypes newTypes = none;
    for (auto typePair : typesMap) {
      if (types & typePair.first) {
        newTypes |= typePair.second;
      }
    }
    return Edge(to, from, newTypes, continueTo);
  };
};

EdgeTypesPair MakeEdgeTypesPair(EdgeTypes first, EdgeTypes second) {
  EdgeTypesPair pair;
  pair.edgeTypes.first = first;
  pair.edgeTypes.second = second;
  return pair;
}

EdgeTypesQuad MakeEdgeTypesQuad(EdgeTypes first, EdgeTypes second=0, EdgeTypes third=0, EdgeTypes fourth=0) {
  EdgeTypesQuad quad;
  quad.edgeTypes.first = first;
  quad.edgeTypes.second = second;
  quad.edgeTypes.third = third;
  quad.edgeTypes.fourth = fourth;
  return quad;
}

EdgeTypesPair MakeEdgeTypesPair(vector<EdgeTypes> vec) {
  assert(vec.size() <= 2, "only two edge type directions allowed");
  unsigned size = vec.size();
  EdgeTypesPair pair = {0};
  if (size > 0) {
    pair.edgeTypes.first = vec[0];
  }
  if (size > 1) {
    pair.edgeTypes.second = vec[1];
  }
  return pair;
}

EdgeTypesQuad MakeEdgeTypesQuad(vector<EdgeTypes> vec) {
  assert(vec.size() <= 4, "only four edge type directions allowed");
  unsigned size = vec.size();
  EdgeTypesQuad pair = {0};
  if (size > 0) {
    pair.edgeTypes.first = vec[0];
  }
  if (size > 1) {
    pair.edgeTypes.second = vec[1];
  }
  if (size > 2) {
    pair.edgeTypes.third = vec[2];
  }
  if (size > 3) {
    pair.edgeTypes.fourth = vec[3];
  }
  return pair;
}

class Graph {
public:
  vector<vector<Edge> > adjList;
  std::map<EdgeTypes,EdgeTypes> transposeMap;
  Graph() { }
  Graph(vector<Edge> const &edges, int count) {
    adjList.resize(count);

    for (auto &edge : edges) {
      addEdge(edge);
    }
  }

  void addEdge(Edge newEdge, bool bidirectional=true) {
    bool forwardFound = false, reverseFound = false;
    for (Edge &edge : adjList[newEdge.from]) {
      if (edge.to == newEdge.to) {
        edge.types |= newEdge.types;
        forwardFound = true;
        break;
      }
    }
    if (bidirectional) {
      for (Edge &edge : adjList[newEdge.to]) {
        if (edge.from == newEdge.from) {
          edge.types |= newEdge.transpose(transposeMap).types;
          reverseFound = true;
          break;
        }
      }
    }
    if (!forwardFound) {
      adjList[newEdge.from].push_back(newEdge);
    }
    if (bidirectional && !reverseFound) {
      adjList[newEdge.to].push_back(newEdge.transpose(transposeMap));
    }
  }

  // TODO: using std::list for adjacency search operations and returns might be a small perf win

  vector<Edge> adjacencies(PixelIndex vertex, EdgeTypesPair pair, bool exactMatch=false) {
    vector<Edge> adjList;
    getAdjacencies(vertex, pair.edgeTypes.first, adjList, exactMatch);
    getAdjacencies(vertex, pair.edgeTypes.second, adjList, exactMatch);
    return adjList;
  }

  vector<Edge> adjacencies(PixelIndex vertex, EdgeTypesQuad quad, bool exactMatch=false) {
    vector<Edge> adjList;
    getAdjacencies(vertex, quad.edgeTypes.first, adjList, exactMatch);
    getAdjacencies(vertex, quad.edgeTypes.second, adjList, exactMatch);
    getAdjacencies(vertex, quad.edgeTypes.third, adjList, exactMatch);
    getAdjacencies(vertex, quad.edgeTypes.fourth, adjList, exactMatch);
    return adjList;
  }

  void getAdjacencies(PixelIndex vertex, EdgeTypes matching, std::vector<Edge> &insertInto, bool exactMatch) {
    if (matching == 0) {
      return;
    }
    vector<Edge> &adj = adjList[vertex];
    for (Edge &edge : adj) {
      auto matchedTypes = (edge.types & matching);
      if ((matchedTypes == matching) || (!exactMatch && matchedTypes)) {
        insertInto.push_back(edge);
      }
    }
  }
};

#endif
