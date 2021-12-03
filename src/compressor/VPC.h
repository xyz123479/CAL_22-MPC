#ifndef __VPC_H__
#define __VPC_H__

#include <ios>
#include <map>

#include "./Compressor.h"
#include "./CompResult.h"
#include "./VPCmodules/CompressionModule.h"
#include "./VPCmodules/FPCModule.h"

namespace comp
{

struct ClusterStat
{
  ClusterStat()
    : count(0), originalSize(0), compressedSize(0), compRatio(0)
  {
    for (int i = 0; i < COMPSIZELIMIT; i++)
      compSizeHistogram.insert(std::make_pair(i, 0));
  }

  uint64_t count;

  uint64_t originalSize;
  uint64_t compressedSize;
  double compRatio;

  std::map<int, uint64_t> compSizeHistogram;
};

struct VPCResult : public CompResult
{
  /*** constructors ***/
  VPCResult(unsigned lineSize)
    : CompResult(lineSize), m_NumModules(0), m_UncompCount(0) {}
  
  VPCResult(unsigned lineSize, int numModules)
    : CompResult(lineSize), m_NumModules(numModules), m_UncompCount(0) 
  {
    SetNumModules(numModules);
  }

  /*** member functions ***/
  virtual void Update(unsigned uncompSize, unsigned compSize, int selected)
  {
    CompResult::Update(uncompSize, compSize);

    if (selected == -1)
    {
      m_UncompCount++;
    }
    else
    {
      ClusterStat &clusterStat = m_ClusterStats[selected];
      clusterStat.originalSize += uncompSize;
      clusterStat.compressedSize += compSize;
      clusterStat.compRatio = (double)clusterStat.originalSize / (double)clusterStat.compressedSize;
      clusterStat.count++;

      clusterStat.compSizeHistogram[compSize]++;
    }
  }

  virtual void Print(std::string workloadName = "", std::string filePath = "")
  {
    // select file or stdout
    std::streambuf *buff;
    std::ofstream file;
    if (filePath == "")  // stdout
    {
      buff = std::cout.rdbuf();
    }
    else  // file
    {
      // write column index description
      if (!isFileExists(filePath))
      {
        file.open(filePath);
        if (!file.is_open())
        {
          std::cout << fmt::format("File is not open: \"{}\"", filePath) << std::endl;
          exit(1);
        }
        // first line
        file << fmt::format("workload,total,,,");
        file << "-1,";  // uncomp
        for (int i = 0; i < m_NumModules; i++)
        {
          file << fmt::format("{},,,", i);
        }
        file << std::endl;

        // second line
        file << ",";
        file << "original_size,compressed_size,compression_ratio,";
        file << "count,";
        for (int i = 0; i < m_NumModules; i++)
        {
          file << "original_size,compressed_size,compression_ratio,";
        }
        file << std::endl;
        file.close();
      }
      file.open(filePath, std::ios_base::app);
      buff = file.rdbuf();
    }
    std::ostream stream(buff);

    // print result
    // workloadname, originalsize, compressedsize, compratio
    stream << fmt::format("{0},{1},{2},{3},", workloadName, OriginalSize, CompressedSize, CompRatio);
    // uncompressed count
    stream << fmt::format("{0},", m_UncompCount);
    for (int i = 0; i < m_NumModules; i++)
    {
      // originalsize, compressedsize, compratio by each module
      ClusterStat &clusterStat = m_ClusterStats[i];
      stream << fmt::format("{0},{1},{2},", clusterStat.originalSize, clusterStat.compressedSize, clusterStat.compRatio);
    }
    stream << std::endl;

    if (file.is_open())
      file.close();
  }

  virtual void PrintDetail(std::string workloadName = "", std::string filePath = "")
  {
    // select file or stdout
    std::streambuf *buff;
    std::ofstream file;
    if (filePath == "")   // stdout
    {
      buff = std::cout.rdbuf();
    }
    else
    {
      // write column index description
      if (!isFileExists(filePath))
      {
        file.open(filePath);
        if (!file.is_open())
        {
          std::cout << fmt::format("File is not open: \"{}\"", filePath) << std::endl;
          exit(1);
        }
        // first line
        file << fmt::format("workload,");
        for (int i = 0; i < m_NumModules; i++)
        {
          file << fmt::format("{},", i);
          for (int j = 0; j < COMPSIZELIMIT-1; j++)
          {
            file << ",";
          }
        }
        file << std::endl;

        // second line
        file << ",";
        for (int i = 0; i < m_NumModules; i++)
        {
          for (int j = 0; j < COMPSIZELIMIT; j++)
          {
            file << fmt::format("{},", j);
          }
        }
        file << std::endl;
        file.close();
      }
      file.open(filePath, std::ios_base::app);
      buff = file.rdbuf();
    }
    std::ostream stream(buff);

    // workloadname
    stream << fmt::format("{0},", workloadName);
    // histogram by each module
    for (int i = 0; i < m_NumModules; i++)
    {
      ClusterStat &clusterStat = m_ClusterStats[i];
      for (int j = 0; j < COMPSIZELIMIT; j++)
      {
        stream << fmt::format("{0},", clusterStat.compSizeHistogram[j]);
      }
    }
    stream << std::endl;
  }

  void SetNumModules(int numModules)
  {
    m_NumModules = numModules;

    for (int i = 0; i < numModules; i++)
    {
      // add stat into the list
      ClusterStat stat;
      m_ClusterStats.push_back(stat);
    }
  }

  /*** member variables ***/
  std::vector<ClusterStat> m_ClusterStats;
  uint64_t m_UncompCount;
  int m_NumModules;

};

class VPC : public Compressor
{
public:
  /*** constructors ***/
  VPC(std::string configPath)
  {
    parseConfig(configPath);
    m_Stat = new VPCResult(m_LineSize, m_NumModules);
    m_Stat->CompressorName = "Contrastive Clustering Compressor";
  }

  /*** getters ***/
  int GetLineSize()    { return m_LineSize; }
  int GetNumModules()  { return m_NumModules; }
  int GetNumClusters() { return m_NumClusters; }

  /*** methods ***/
  virtual unsigned CompressLine(std::vector<uint8_t> &dataLine);

private :
  void parseConfig(std::string &configPath);
  unsigned compressLineOnlyAllZero(std::vector<uint8_t> &dataLine);
  unsigned compressLineAllWordSame(std::vector<uint8_t> &dataLine);

private:
  /*** members ***/
  unsigned (VPC::*compressLine)(std::vector<uint8_t> &dataLine);

  int m_LineSize;
  std::map<int, int> m_EncodingBits;

  std::vector<CompressionModule*> m_CompModules;
  FPCModule m_CommonEncoder;
  int m_NumModules;
  int m_NumClusters;
};

}

#endif  // __VPC_H__
