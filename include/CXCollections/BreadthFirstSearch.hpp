#pragma once

#include <stdint.h>
#include <vector>
#include <deque>
#include <unordered_map>

namespace cyber 
{

struct BFSNode
{
    BFSNode* pNext; // holds next neighbor, or next in path
};

// each instance of search object holds heap memory for path and search temp values
// expected usage is to re-use the same class object so that allocated memory is reused
class BreadthFirstSearch
{
public:
    // returns path as vector of node pointers or nullptr if no path found
    // path is only valid until called again
    // if need to call this function multiple times before using path, then copy vector data out
    const std::vector<BFSNode*>* FindPathReversed(BFSNode* pStart, BFSNode* pEnd);

private:
    std::vector<BFSNode*> path;
    std::deque<BFSNode*> frontier;
    std::unordered_map<BFSNode*, BFSNode*> node_to_prev_node;
};

}