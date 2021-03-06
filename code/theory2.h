/*
  Graph: 
    A -> B
    A -> C
    B -> D
    C -> D
  
  Schedule:
    1 A B D D .. 
    2 A C D C ..
    3 A B   D ..
    4 A C C   ..
    5   B B   ..


*/
#include <cstdio>
#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <cassert>

using namespace std;

#define MAXN 50000
#define MINN 70
#define MAXR 500
#define MOD 100

#define LZD 0

typedef pair<int, int> P;

void testInput() {
  int t;
  scanf("%d", &t);
}

struct RunningNode {
  int id;
  char name[2 * MINN];
  char op[2 * MINN];
  double cost;

  int round;
  int peid;       // run on which pe
  double starttime;
  double endtime;

  RunningNode() {
    round = peid = -1;
    starttime = endtime = -1;
  }

  void setTime(double st, double ed) {
    starttime = st;
    endtime = ed;
  }

  friend bool operator< (RunningNode a, RunningNode b) {
    if (a.endtime != b.endtime)
      return a.endtime > b.endtime;
    return a.peid > b.peid;
  }
};

struct WaitingNode {
  int id;
  double cost;

  int round;
  double starttime;
  double comparevalue;
  int topologyorder;

  friend bool operator< (WaitingNode a, WaitingNode b) {
    if (a.topologyorder != b.topologyorder)
      return a.topologyorder > b.topologyorder;
    // if (a.comparevalue != b.comparevalue)
    //   return a.comparevalue < b.comparevalue;
    if (a.cost != b.cost)
      return a.cost > b.cost;
    return a.round > b.round;
  }
};

struct Edge {
  int from;
  int to;
  double cost;
  double memory;

  Edge() { }

  Edge(int a, int b, double c, double d) {
    from = a;
    to = b;
    cost = c;
    memory = d;
  }
};

vector<Edge> edgelist[MAXN];
RunningNode nodelist[MAXN];
bool vis[MAXN][MAXR];
int degree[MAXN][MAXR];
int total_node;
double edpe[MINN];
double nextvalue[MAXN];
int indegree[MAXN];
int maptopology[MAXN];

void initTopologyMap() {
  int count = 0, iter = 0;
  queue<RunningNode> q;
  for (int i = 1; i <= total_node; i++) {
    if (indegree[i] == 0) {
      q.push(nodelist[i]);
    }
  }
  count = q.size();

  while (!q.empty()) {
    RunningNode f = q.front();
    q.pop();
    maptopology[f.id] = iter;

    count = count - 1;

    for (int i = 0; i < edgelist[f.id].size(); i++) {
      Edge e = edgelist[f.id][i];
      indegree[e.to] = indegree[e.to] - 1;
      if (indegree[e.to] == 0) {
        q.push(nodelist[e.to]);
      }
    }

    if (count == 0) {
      count = q.size();
      iter = iter + 1;
    }
  }
}

void init(int period_times) {
  memset(vis, false, sizeof(vis));
  memset(degree, 0, sizeof(degree));
  memset(edpe, 0, sizeof(edpe));
  memset(nextvalue, 0, sizeof(nextvalue));
  memset(indegree, 0, sizeof(indegree));

  for (int i = 1; i <= total_node; i++) {
    double maxvalue = 0;
    for (int j = 0; j < edgelist[i].size(); j++) {
      Edge e = edgelist[i][j];
      maxvalue = max(e.cost, maxvalue);
      for (int k = 1; k <= period_times; k++)
        degree[e.to][k] = degree[e.to][k] + 1;
        indegree[e.to] = indegree[e.to] + 1;
    }
    nextvalue[i] = maxvalue;
  }

  initTopologyMap();
}


void solve(int total_pe, int period_times) {
  init(period_times);

  double total_time = 0;
  int pecount = 0, node_count = 0;
  priority_queue<RunningNode> perunning;
  priority_queue<WaitingNode> nodewaiting;
  priority_queue<int, vector<int>, greater<int> > freepe;
  // init free PE queue
  for (int i = 0; i < total_pe; i++)
    freepe.push(i);

  // first calculate those nodes whose indegree is zero
  // if PE is not enough, push those nodes into nodewaiting queue
  for (int j = 1; j <= period_times; j++) {
    for (int i = 1; i <= total_node; i++) {
      if (degree[i][j] == 0) {
        RunningNode n = nodelist[i];
        if (!freepe.empty()) {
          int peid = freepe.top();
          freepe.pop();
          pecount = max(pecount, peid);
          
          
          n.round = j;
          n.setTime(0, n.cost);
          n.peid = peid;

          perunning.push(n);
        }
        else {
          WaitingNode wn = WaitingNode();
          wn.id = n.id;
          wn.cost = n.cost;
          wn.round = j;
          wn.starttime = 0;
          wn.comparevalue = wn.cost + nextvalue[wn.id];
          wn.topologyorder = maptopology[wn.id];
          nodewaiting.push(wn);
        }
        vis[n.id][j] = true;
      }
    }
  }

  #if LZD == 1
    int total_edge = 0;
    for (int i = 1; i <= total_node; i++)
      total_edge = total_edge + edgelist[i].size();
    printf("%d %d\n", total_node, total_edge);
  #endif
  // then calculate those running nodes
  while (!perunning.empty()) {
    // finish the top node
    RunningNode top = perunning.top();
    perunning.pop();
    #if LZD == 1
      printf("%c%d\t%d\t%.3lf\t%.3lf\n", (top.id + 'A' - 1), top.round, top.peid, top.starttime, top.endtime);
    #endif
    total_time = max(top.endtime, total_time);
    edpe[top.peid] = max(edpe[top.peid], top.endtime);
    // free PE
    freepe.push(top.peid);

    // minus the corresponding round indegree of those dependent nodes 
    for (int i = 0; i < edgelist[top.id].size(); i++) {
      Edge e = edgelist[top.id][i];
      if (!vis[e.to][top.round]) {
        degree[e.to][top.round] = degree[e.to][top.round] - 1;
        if (degree[e.to][top.round] == 0) {
          WaitingNode wn = WaitingNode();
          wn.id = nodelist[e.to].id;
          wn.cost = nodelist[e.to].cost;
          wn.starttime = top.endtime + e.cost;
          wn.round = top.round;
          wn.comparevalue = wn.cost + nextvalue[wn.id];
          wn.topologyorder = maptopology[wn.id];
          nodewaiting.push(wn);
          vis[e.to][top.round] = true;
        }
      }
    }

    // calculate waiting nodes
    while (!nodewaiting.empty()) {
      if (freepe.empty())
        break;
      
      WaitingNode waited = nodewaiting.top();
      nodewaiting.pop();

      int peid = freepe.top();
      freepe.pop();
      // printf("PE:%d Ready for ID:%d Round:%d\n\n", peid, waited.id, waited.round);
      pecount = max(pecount, peid);
      double starttime = max(top.endtime, waited.starttime);
      
      RunningNode rn = RunningNode();
      rn.id = waited.id;
      rn.cost = waited.cost;
      rn.round = waited.round;
      rn.peid = peid;
      rn.setTime(starttime, starttime + waited.cost);
      perunning.push(rn);
    }
  }

  double up = 0;
  for (int i = 1; i <= total_node; i++) {
    up = up + nodelist[i].cost;
  }
  up = up * period_times;
  double down = 0;
  for (int i = 0; i <= pecount; i++) {
    down = down + edpe[i];
  }
  assert(down != 0);
  double cpuratio = up / down;

  #if LZD == 1
    for (int i = 1; i <= total_node; i++) {
      for (int j = 0; j < edgelist[i].size(); j++)
        printf("%d %d %.3lf\n", edgelist[i][j].from, edgelist[i][j].to, edgelist[i][j].memory);
    }
    printf("%d %d\n", pecount + 1, total_node);
    printf("Total PE:\t%d\nTotal time:\t%.2f\nCPU Used Ratio:\t%.2f\n", pecount + 1, (total_time) / (1), cpuratio);
  #else
    printf("Total PE:\t%d\nTotal time:\t%.2f\nCPU Used Ratio:\t%.2f\n", pecount + 1, (total_time) / (1e6), cpuratio);
  #endif
}