#ifndef __LOCALMUSICSOURCE__
#define __LOCALMUSICSOURCE__

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

//libav
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>

#include "AbstractMusicSource.h"

class LocalMusicSource : public AbstractMusicSource{
 private:

  std::string m_path;
  //Contexte
  AVFormatContext * m_pFormatCtx;
  AVStream * m_pStreamAudio;
  AVCodecContext * m_pAudioDecCtx;
  
  //Données en traitement
  AVPacket m_currentPacket;
  AVPacket m_workingPacket;
  AVFrame * m_pCurrentFrame;
  uint8_t ** m_aFrameData;
  int m_unpaddedLinesize;
  uint8_t * m_aBuffer;
  int m_bufferSize;
  int m_availableData;

  //Conditions d'arret
  bool m_noPacketLeft;
  bool m_noFrameLeft;
  bool m_halt;

  //Multi-thread
  std::thread * m_pBufferFeeder;
  std::mutex m_mutex;
  std::condition_variable m_condVar;

  //Fonctions utiles
  int fetchPacket();
  int fetchFrame();
  void processPlanarData();
  void processNonPlanarData();

 public:
  LocalMusicSource(std::string path);
  ~LocalMusicSource();
  void start();
  void stop();
  int getData(uint8_t * data, const int maxData);
  void feedInternalBuffer();
  bool isFinished() const;
  unsigned int getSampleRate() const;
  unsigned int getChannelCount() const;
  unsigned int getSampleSize() const;
};

#endif //  __LOCALSOURCEPLAYER__
