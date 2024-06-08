#pragma once

// std
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
// anari
#include "anari/anari_cpp.hpp" // ours
#include "mesh.h"
#include "Partitioner.h"

namespace util {
  // ======================================================
  // Given a rank, comm-size and .tri file name loads
  // the portino of the mesh assigned to this process
  // ======================================================
  struct PartitionedMeshLoader
  {
    Mesh::SP load(std::string fileName, int commRank, int commSize) {
      std::vector<Cluster> clusters;

      // Load binary tris
      Mesh::SP triMesh = std::make_shared<Mesh>();
      Geometry::SP geom = std::make_shared<Geometry>();
      uint64_t numClusters, numVerts;

      std::ifstream ifs(fileName,std::ios::binary);
      if (!ifs.good()) {
        std::cerr << "cannot open file: " << fileName << '\n';
        return nullptr;
      }
      ifs.read((char *)&numClusters,sizeof(numClusters));
      ifs.read((char *)&triMesh->bounds,sizeof(triMesh->bounds));
      ifs.read((char *)&numVerts,sizeof(numVerts));
      geom->vertex.resize(numVerts);
      ifs.read((char *)geom->vertex.data(),sizeof(float3)*numVerts);

      clusters.resize(numClusters);

      uint64_t clustersPos = ifs.tellg();

      for (unsigned i=0; i<numClusters; ++i) {
        uint64_t numIndices;
        box3 domainBounds;
        ifs.read((char *)&numIndices,sizeof(numIndices));
        ifs.read((char *)&domainBounds,sizeof(domainBounds));
        uint64_t cur = ifs.tellg();
        ifs.seekg(cur+sizeof(int3)*numIndices);
        clusters[i] = {
          (int)i, // clusterID
          -1, // rankID; we don't know this yet
          domainBounds
        };
      }

      auto partitioner = std::make_shared<Partitioner>(clusters,commSize);
      partitioner->partitionRoundRobin();

      ifs.seekg(clustersPos);

      std::vector<unsigned> myClusters;
      size_t myNumTriangles = 0;
      for (unsigned i=0; i<numClusters; ++i) {
        uint64_t numIndices;
        box3 domainBounds;
        ifs.read((char *)&numIndices,sizeof(numIndices));
        if (partitioner->assignedTo(i,commRank)) {
          ifs.read((char *)&domainBounds,sizeof(domainBounds));
          geom->index.resize(numIndices);
          ifs.read((char *)geom->index.data(),sizeof(int3)*numIndices);
          triMesh->geoms.push_back(geom);
          geom = std::make_shared<Geometry>();

          myClusters.push_back(i);
          myNumTriangles += numIndices;
        } else {
          ifs.read((char *)&domainBounds,sizeof(domainBounds));
          uint64_t cur = ifs.tellg();
          ifs.seekg(cur+sizeof(int3)*numIndices);
        }
      }
      std::stringstream s;
      s << "Clusters assigned to (commRank): ("
        << commRank << ")\n\t";
      for (size_t i=0; i<myClusters.size(); ++i) {
        s << myClusters[i];
        if (i < myClusters.size()-1)
          s << ", ";
        else
          s << '\n';
      }
      s << "\t# clusters on (" << commRank << "): "
        << myClusters.size() << '\n';
      s << "\t# triangles on (" << commRank << "): "
        << prettyNumber(myNumTriangles) << '\n';
      std::cout << s.str();

      return triMesh;
    }

    // ====================================================
    // remove vertices not used by this geometry
    // ====================================================
    Mesh::SP compactMesh(Mesh::SP input) {
      for (size_t i=0; i<input->geoms.size(); ++i) {
        for (size_t j=i+1; j<input->geoms.size(); ++j) {
          if (input->geoms[i]->vertex == input->geoms[j]->vertex) {
            std::cerr << "skipping compaction, only implemented for multi-geoms "
                << "without *shared* vertex arrays!\n";
            return input;
          }
        }
      }

      for (size_t i=0; i<input->geoms.size(); ++i) {
        auto &geom = input->geoms[i];
        size_t numTriangles = geom->index.size();
        std::vector<float3> ourVertices(numTriangles*3);
        size_t index=0;
        for (size_t j=0; j<geom->index.size(); ++j) {
          int3 idx(index,index+1,index+2);
          ourVertices[idx.x] = geom->vertex[geom->index[i].x];
          ourVertices[idx.y] = geom->vertex[geom->index[i].y];
          ourVertices[idx.z] = geom->vertex[geom->index[i].z];
          geom->index[i] = idx;
          index += 3;
        }
        geom->vertex = ourVertices;
      }

      return input;
    }

    std::vector<anari::Geometry> loadANARI(anari::Device device,
                                           std::string fileName,
                                           int commRank,
                                           int commSize,
                                           box3 *bounds=NULL) {

      std::vector<anari::Geometry> res;
      auto triMesh = load(fileName, commRank, commSize);

      for (size_t i=0; i<triMesh->geoms.size(); ++i) {
        const Geometry::SP &geom = triMesh->geoms[i];
        auto ageom = anari::newObject<anari::Geometry>(device, "triangle");

        anari::Array1D data;
        data = anari::newArray1D(device, geom->vertex.data(), geom->vertex.size());
        anari::setAndReleaseParameter(device, ageom, "vertex.position", data);

        using uint3 = anari::math::uint3;
        data = anari::newArray1D(device, (uint3 *)geom->index.data(), geom->index.size());
        anari::setAndReleaseParameter(device, ageom, "primitive.index", data);

        anari::commitParameters(device, ageom);
        res.push_back(ageom);
      }

      if (bounds) {
        *bounds = triMesh->bounds;
      }

      return res;
    }

    /*! return a nicely formatted number as in "3.4M" instead of
        "3400000", etc, using mulitples of thousands (K), millions
        (M), etc. Ie, the value 64000 would be returned as 64K, and
        65536 would be 65.5K */
    inline std::string prettyNumber(const size_t s)
    {
      char buf[1000];
      if (s >= (1000LL*1000LL*1000LL*1000LL)) {
        snprintf(buf, 1000,"%.2fT",s/(1000.f*1000.f*1000.f*1000.f));
      } else if (s >= (1000LL*1000LL*1000LL)) {
        snprintf(buf, 1000, "%.2fG",s/(1000.f*1000.f*1000.f));
      } else if (s >= (1000LL*1000LL)) {
        snprintf(buf, 1000, "%.2fM",s/(1000.f*1000.f));
      } else if (s >= (1000LL)) {
        snprintf(buf, 1000, "%.2fK",s/(1000.f));
      } else {
        snprintf(buf,1000,"%zi",s);
      }
      return buf;
    }

  };
} // util
