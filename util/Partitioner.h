#pragma once

// std
#include <limits.h>
#include <iostream>
#include <numeric>
#include <vector>
// anari
#include "anari/anari_cpp/ext/linalg.h"
// ours
#include "box3.h"

namespace util {

  using float3 = anari::math::float3;
  using mat4 = anari::math::mat4;
  using box3 = anari::math::box3;

  struct Cluster {
    // unique id of this cluster
    int id;

    // rank that the cluster is on
    int rank;

    // domain bounds; those won't overlap
    box3 domain;
  };

  // ==================================================================
  // Partition clusters to ranks
  // ==================================================================

  struct Partitioner
  {
    Partitioner(std::vector<Cluster> &clusters, int numRanks)
      : input(clusters)
      , numRanks(numRanks)
    {
      compositeOrder.resize(numRanks);
      std::iota(compositeOrder.begin(),compositeOrder.end(),0);
      perRank.resize(numRanks);
    }

    void partitionRoundRobin()
    {
      auto div_up = [](int a, int b) { return (a+b-1)/b; };
      int numClustersPerRank = div_up(input.size(),numRanks);
      for (size_t i=0; i<input.size(); ++i)
      {
        int clusterID = i;
        int rankID = i/numClustersPerRank;
        input[i].id = clusterID;
        input[i].rank = rankID;
        perRank[rankID].push_back(clusterID);
      }
    }

    void partitionKD()
    {
      // Assign same number of clusters per rank; use split-middle heuristic
      int numClustersPerRank = input.size()/numRanks;

      struct Clusters {
        box3 bounds;
        std::vector<int> clusterIDs;
        int kdNodeID;
      };

      Clusters all;
      all.bounds = { float3(1e30f), float3(-1e30f) };
      for (size_t i=0; i<input.size(); ++i)
      {
        all.bounds.extend(input[i].domain);
        all.clusterIDs.push_back(input[i].id);
        all.kdNodeID = 0;
      }

      std::vector<Clusters> clusters;
      std::vector<Clusters> assignedClusters;

      clusters.push_back(all);

      kdTree.emplace_back(KDNode{0,0.f,INT_MAX,INT_MAX});

      while (!clusters.empty()) {
        unsigned clusterToPick = 0;

        // Pick cluster with most clusters
        int maxNumIDs = 0;
        for (unsigned i=0; i<clusters.size(); ++i)
        {
          if (clusters[i].clusterIDs.size() > maxNumIDs) {
            clusterToPick = i;
            maxNumIDs = clusters[i].clusterIDs.size();
          }
        }

        Clusters cs = clusters[clusterToPick];
        clusters.erase(clusters.begin()+clusterToPick);

        if (cs.clusterIDs.size() <= numClustersPerRank
         || assignedClusters.size() == numRanks-1) {
          int rankID = assignedClusters.size();
          kdTree[cs.kdNodeID].child1 = ~rankID;
          kdTree[cs.kdNodeID].child2 = ~rankID;
          assignedClusters.push_back(cs);

          if (assignedClusters.size() == numRanks) {
            // Bump all remaining clusters into the last one
            Clusters& last = assignedClusters.back();
            while (!clusters.empty()) {
              Clusters cs = clusters[0];
              for (auto id : cs.clusterIDs) {
                last.clusterIDs.push_back(id);
              }
              clusters.erase(clusters.begin());
            }
            break;
          }
          else
            continue;
        }

        int splitAxis = 0;
        if (cs.bounds.size()[1]>cs.bounds.size()[0]
         && cs.bounds.size()[1]>=cs.bounds.size()[2]) {
          splitAxis = 1;
        } else if (cs.bounds.size()[2]>cs.bounds.size()[0]
                && cs.bounds.size()[2]>=cs.bounds.size()[1]) {
          splitAxis = 2;
        }

        // Middle split
        float splitPlane = cs.bounds.lower[splitAxis]+cs.bounds.size()[splitAxis]*.5f;

        Clusters L,R;
        L.bounds = { float3(1e30f), float3(-1e30f) };
        R.bounds = { float3(1e30f), float3(-1e30f) };
        for (size_t i=0; i<cs.clusterIDs.size(); ++i) {
          int clusterID = cs.clusterIDs[i];
          Cluster c = input[clusterID];
          float3 centroid = c.domain.center();
          if (centroid[splitAxis] < splitPlane) {
            L.bounds.extend(c.domain);
            L.clusterIDs.push_back(clusterID);
          } else {
            R.bounds.extend(c.domain);
            R.clusterIDs.push_back(clusterID);
          }
        }

        kdTree[cs.kdNodeID].splitAxis = splitAxis;
        kdTree[cs.kdNodeID].splitPlane = splitPlane;

        L.kdNodeID = kdTree.size();
        kdTree[cs.kdNodeID].child1 = L.kdNodeID;
        kdTree.emplace_back(KDNode{0,0.f,INT_MAX,INT_MAX});
        clusters.push_back(L);

        R.kdNodeID = kdTree.size();
        kdTree[cs.kdNodeID].child2 = R.kdNodeID;
        kdTree.emplace_back(KDNode{0,0.f,INT_MAX,INT_MAX});
        clusters.push_back(R);
      }

      for (size_t i=0; i<assignedClusters.size(); ++i) {
        for (size_t j=0; j<assignedClusters[i].clusterIDs.size(); ++j) {
          perRank[i].push_back(assignedClusters[i].clusterIDs[j]);
        }
      }

      // for (auto kd : kdTree) {
      //   std::cout << kd.splitAxis << ','
      //             << kd.splitPlane << ','
      //             << kd.child1 << ','
      //             << kd.child2 << '\n';
      // }
    }

    bool assignedTo(int clusterID, int rankID)
    {
      for (size_t i=0; i<perRank[rankID].size(); ++i) {
        if (perRank[rankID][i]==clusterID)
          return true;
      }
      return false;
    }

    void computeCompositeOrder(const float3 &reference)
    {
      if (kdTree.empty()) {
        std::cerr << "Warning: arbitrary composite order!\n";
        return;
      }

      compositeOrder.clear();

      std::vector<int> stack;
      int addr = 0;
      stack.push_back(addr);

      while (!stack.empty()) {
        KDNode node = kdTree[addr];

        if (node.child1 < 0 && node.child2 < 0) {
          int rankID = ~node.child1;
          assert(rankID==~node.child2);
          compositeOrder.push_back(rankID);
          addr = stack.back();
          stack.pop_back();
        } else if (node.child1 == INT_MAX) {
          addr = stack.back();
          stack.pop_back();
        } else {
          if (reference[node.splitAxis] < node.splitPlane) {
            addr = node.child1;
            stack.push_back(node.child2);
          } else {
            addr = node.child2;
            stack.push_back(node.child1);
          }
        }
      }
    }

    struct KDNode {
      int splitAxis;
      float splitPlane;
      int child1,child2;
    };

    // KD tree to sort clusters into visibility order
    std::vector<KDNode> kdTree;

    // Input clusters
    std::vector<Cluster> &input;

    // Number of ranks (input)
    int numRanks;

    // Per-rank clusterIDs
    std::vector<std::vector<int>> perRank;

    // rankIDs in composite order
    std::vector<int> compositeOrder;
  };
} // util
