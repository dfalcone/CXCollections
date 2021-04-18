#include "..\include\CXCollections\BreadthFirstSearch.hpp"

namespace cyber
{

const std::vector<BFSNode*>* BreadthFirstSearch::FindPathReversed(BFSNode* pStart, BFSNode* pEnd)
{
    path.clear();
    frontier.clear();
    node_to_prev_node.clear();

    frontier.push_back(pStart);
    node_to_prev_node[pStart] = nullptr;

    BFSNode* pCurNode;
    BFSNode* pNextNode;

    while (!frontier.empty())
    {
        pCurNode = frontier.front();
        frontier.pop_front();

        if (pCurNode == pEnd)
        {
            // return first found in reverse order
            while (pCurNode)
            {
                path.push_back(pCurNode);
                pCurNode = node_to_prev_node[pCurNode];
            }

            return &path;
        }

        pNextNode = pCurNode->pNext;
        while (pNextNode)
        {
            auto prevItr = node_to_prev_node.find(pNextNode);
            // if itr is end, does not exist
            if (prevItr == node_to_prev_node.end())
            {
                frontier.push_back(pNextNode);
                // assign cur as prev to neighbor
                prevItr->second = pCurNode;
            }
        }
    }

    return nullptr;
}

}