#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <unordered_set>
#include <vector>

using namespace std;  

struct NextHop {
    int nextHop[2];
};

ofstream outfile;

class Node {
    private:
        int ID;
    public:
        map<int, int> adjMap;
        map<int, NextHop> forwarding;        //if you want to change this value later, no need to make it private
        Node(int self, int neighbor, int weight);
        void addAdj(int neighbor, int weight);
        int getID(){return ID;};             //directly return is return value not reference!!!!!!!
        void printForward();
};

Node::Node(int self, int neighbor, int weight) {
    ID = self;
    NextHop selfEntry = {self, 0};
    forwarding[self] = selfEntry;
    addAdj(neighbor, weight);
}

void Node::addAdj(int neighbor, int weight) {
    adjMap[neighbor] = weight;
    NextHop entry = {neighbor, weight};
    forwarding[neighbor] = entry;
}

void Node::printForward() {
    //cout<<"My id is: "<<ID<<endl;
    for (map<int, NextHop>::iterator iter = forwarding.begin(); iter != forwarding.end(); iter++) {
        outfile<<iter->first<<" "<<iter->second.nextHop[0]<<" "<<iter->second.nextHop[1]<<endl;
    }
    outfile<<endl;
}

class Graph {
    private:
        map<int, Node> nodes;
        void addEdge(int either, int other, int weight);
        void updateEdge(int either, int other, int weight);
        unordered_set<int> BFset;
        void findShorestPath(int src, int dest, vector<int> &path);
    public:
        void construct(char* topofile);
        void BellFord();
        void relax(Node &node);     //must use & for reference transfer
        bool through(int stopID, int destID, int selfID);
        void printNodes();
        void update(char* changesfile);
        void sendMsg(char* messagefile);
        char* messageFileName;
};

void Graph::construct(char* topoFileName) {
    ifstream topofile;
    topofile.open(topoFileName, ios::in);
    string line;
    while(getline(topofile, line)) {  //split the topofile line
        int pos = line.find(" ");
        int either = stoi(line.substr(0, pos));
        line = line.substr(pos+1, line.size());
        pos = line.find(" ");
        int other = stoi(line.substr(0, pos));
        int weight = stoi(line.substr(pos+1, line.size()));
        addEdge(either, other, weight);
        addEdge(other, either, weight);
    }
    topofile.close();
    BellFord();
}

void Graph::update(char* changesFileName) {
    ifstream changefile;
    changefile.open(changesFileName, ios::in);
    string line;
    while(getline(changefile, line)) {
        int pos = line.find(" ");
        int either = stoi(line.substr(0, pos));
        line = line.substr(pos+1, line.size());
        pos = line.find(" ");
        int other = stoi(line.substr(0, pos));
        int weight = stoi(line.substr(pos+1, line.size()));
        updateEdge(either, other, weight);
        updateEdge(other, either, weight);
        BellFord();
    }
    changefile.close();
}

void Graph::addEdge(int either, int other, int weight) {
    map<int, Node>::iterator iter;
    iter = nodes.find(either);
    if (iter == nodes.end()) {
        Node newNode(either, other, weight);
        nodes.insert(map<int, Node>::value_type(either, newNode));  //note! must use insret to insert self defined class; and insert() will not overwrite!!
        BFset.insert(either);
    } else {
        iter->second.addAdj(other, weight);
    }
}

void Graph::updateEdge(int either, int other, int weight) {
    Node &curNode = nodes.find(either)->second;    //must use & if you want a reference
    if (weight == -999) {
        //delete the adj edge
        curNode.adjMap.erase(other);
    } else {
        //update new weights
        curNode.adjMap[other] = weight;
        curNode.forwarding[other].nextHop[0] = other;
        curNode.forwarding[other].nextHop[1] = weight;
    }
    //just insert the node itself, leave the rest to bellford
    BFset.insert(curNode.getID());
}

//use to build or update the forwarding table
void Graph::BellFord() {
    map<int, Node>::iterator node;
    while (!BFset.empty()) {
        unordered_set<int> curSet = BFset;   //in std, default "=" is a deep copy
        BFset.clear();
        for (unordered_set<int>::iterator iter = curSet.begin(); iter != curSet.end(); iter++) {
            relax(nodes.find(*iter)->second);
        }
    }
    printNodes();
    sendMsg(messageFileName);
}

void Graph::relax(Node &node) {
    map<int, Node>::iterator dest;
    for (dest = nodes.begin(); dest != nodes.end(); dest++) {
        if (node.getID() == dest->second.getID()) continue;  //never need to update self forwarding
        int cost = INT16_MAX;
        int nextHopID = INT16_MAX;
        int oriCost = cost;
        int oriHopID = nextHopID;
        int destID = dest->second.getID();
        map<int, NextHop>::iterator dirct = node.forwarding.find(destID);
        if (dirct != node.forwarding.end()) {  //if the forwarding table contains this dest cost
            oriCost = dirct->second.nextHop[1];
            oriHopID = dirct->second.nextHop[0];
        }
        for (map<int, int>::iterator neighbor = node.adjMap.begin(); neighbor != node.adjMap.end(); neighbor++) {  //dxY = min(c(x, v) + dvY)
            Node &adjNode = nodes.find(neighbor->first)->second;
            map<int, NextHop>::iterator hop = adjNode.forwarding.find(destID);
            if (hop != adjNode.forwarding.end() && !through(hop->second.nextHop[0], destID, node.getID())) {  //poissoned reverse
                int firstStep = neighbor->second;
                int pathCost = hop->second.nextHop[1] + firstStep;
                if (pathCost < cost || (pathCost == cost && adjNode.getID() < nextHopID)) {  //if need to update the forwarding table
                    cost = pathCost;
                    nextHopID = adjNode.getID();
                }
            }
        }
        //every time calculate nodes' cost to every dest and see if the forwarding table changes
        if (cost != oriCost || nextHopID != oriHopID) {
            if (cost == INT16_MAX) {
                node.forwarding.erase(destID);
            } else {
                NextHop newHop = {nextHopID, cost};
                node.forwarding[destID] = newHop;  //only this kind of insert---[] can overwrite the value
            }
            //only relax nodes whose neighbor's forwarding table changed in the previous iteration
            for (map<int, int>::iterator neighbor = node.adjMap.begin(); neighbor != node.adjMap.end(); neighbor++) {
                BFset.insert(neighbor->first);
            }
        }
    }
}

//check if the path that from self to dest through the selfID
bool Graph::through(int stopID, int destID, int selfID) {
    if (stopID == selfID) return true;
    Node stop = nodes.find(stopID)->second;
    while (stopID != destID) {
        map<int, NextHop>::iterator stopIter = stop.forwarding.find(destID);
        if (stopIter == stop.forwarding.end()) return true;  //pass through self or path break, both shouldn't update forwarding table
        stopID = stopIter->second.nextHop[0];
        if (stopID == selfID) return true;
        stop = nodes.find(stopID)->second;
    }
    return false;
}

void Graph::sendMsg(char* messageFileName) {
    ifstream topofile;
    topofile.open(messageFileName, ios::in);
    string line;
    while(getline(topofile, line)) {  //split the topofile line
        int pos = line.find(" ");
        int src = stoi(line.substr(0, pos));
        line = line.substr(pos+1, line.size());
        pos = line.find(" ");
        int dest = stoi(line.substr(0, pos));
        string text = line.substr(pos+1, line.size());
        Node &srcNode = nodes.find(src)->second;
        if (srcNode.forwarding.find(dest) != srcNode.forwarding.end()) {
            int cost = srcNode.forwarding.find(dest)->second.nextHop[1];
            vector<int> path;
            findShorestPath(src, dest, path);
            outfile<<"from "<<src<<" to "<<dest<<" cost "<<cost<<" hops ";
            for (int i = 0; i < path.size(); i++) {
                outfile<<path[i]<<" ";
            }
            outfile<<"message "<<text<<endl;
        } else {
            outfile<<"from "<<src<<" to "<<dest<<" cost infinite hops unreachable message "<<text<<endl;
        }
    }
    topofile.close();
}

void Graph::findShorestPath(int src, int dest, vector<int> &path) {
    Node curNode = nodes.find(src)->second;
    while (curNode.getID() != dest) {
        path.push_back(curNode.getID());
        int nextStep = curNode.forwarding.find(dest)->second.nextHop[0];
        curNode = nodes.find(nextStep)->second;
    }
}


void Graph::printNodes() {
    map<int, Node>::iterator iter;
    for (iter = nodes.begin(); iter != nodes.end(); iter++) {
        iter->second.printForward();
    }
}



int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }
    
    outfile.open("output.txt", ios::out | ios::trunc);
    Graph g;
    g.messageFileName = argv[2];
    g.construct(argv[1]);
    g.update(argv[3]);

    outfile.close();
    return 0;
}

