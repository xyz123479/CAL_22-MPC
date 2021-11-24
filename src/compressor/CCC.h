#ifndef __CCC_H__
#define __CCC_H__

#include <ios>
#include <map>

#include "./Compressor.h"
#include "./CompResult.h"
#include "./CCCmodules/CompressionModule.h"
#include "./CCCmodules/FPCModule.h"

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

struct CCCResult : public CompResult
{
  /*** constructors ***/
  CCCResult()
    : CompResult(), m_NumModules(0), m_UncompCount(0) {}
  
  CCCResult(int numModules)
    : CompResult(), m_NumModules(numModules), m_UncompCount(0) 
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

class CCC : public Compressor
{
public:
  /*** constructors ***/
  CCC(std::string configPath)
  {
    m_Stat = new CCCResult();
    m_CompName = "Contrastive Clustering Compressor";
    m_Stat->CompressorName = m_CompName;
    parseConfig(configPath);
  }

  /*** getters ***/
  int GetLineSize()    { return m_LineSize; }
  int GetNumModules()  { return m_NumModules; }
  int GetNumClusters() { return m_NumClusters; }

  /*** methods ***/
  virtual unsigned CompressLine(std::vector<uint8_t> &dataLine);

private :
  void parseConfig(std::string &configPath);

private:
  /*** members ***/
  int m_LineSize;
  std::map<int, int> m_EncodingBits;

  std::vector<CompressionModule*> m_CompModules;
  FPCModule m_CommonEncoder;
  int m_NumModules;
  int m_NumClusters;
};

}

#endif  // __CCC_H__
