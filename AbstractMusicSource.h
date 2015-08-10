#ifndef __ABSTRACTMUSICSOURCE__
#define __ABSTRACTMUSICSOURCE__

#include <string>
#include <cstdint>

//Temps avanat que la lecture d'un fichier soit abandonnée en cas d'erreur, en secondes
#define TIME_OUT 10

class AbstractMusicSource {
 public:
  virtual ~AbstractMusicSource() = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual int getData(uint8_t * data, const int maxData) = 0;
  virtual void feedInternalBuffer() = 0;
  virtual bool isFinished() const = 0;
  virtual unsigned int getSampleRate() const = 0;
  virtual unsigned int getChannelCount() const = 0;
  virtual unsigned int getSampleSize() const = 0;

};

#endif // __ABSTRACTMUSICSOURCE__
