#include "LocalMusicSource.h"

using namespace std;

LocalMusicSource::LocalMusicSource(string path):m_path(path),m_availableData(0), m_noPacketLeft(false), m_noFrameLeft(false), m_halt(true){

}

LocalMusicSource::~LocalMusicSource(){
  stop();
}

void LocalMusicSource::start(){
  if (m_halt){
    // Ouvre le fichier et stocke les infos de son header
    // Retourne 0 si tout va bien
    if(avformat_open_input(&m_pFormatCtx, m_path.c_str(), NULL, NULL) < 0){
      throw "Impossible d'ouvrir le fichier ";
    }
    
    if(avformat_find_stream_info(m_pFormatCtx, NULL) < 0){
      throw "Impossible de trouver les informations de stream du fichier ";
    }
    
    // Vérifie que le fichier n'est pas une video
    if (av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0) >= 0){
      avformat_close_input(&m_pFormatCtx);
      throw "Erreur : fichier video. Seuls les fichiers audios sont acceptés";
    }
    
    // Trouve le stream correspondant à l'audio
    int streamNb = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (streamNb < 0){
      avformat_close_input(&m_pFormatCtx);
      throw "Stream audio introuvable dans le fichier\n";
    }
    AVStream * pStreamAudio = m_pFormatCtx->streams[streamNb];
    
    //Trouve le decodeur pour le stream
    m_pAudioDecCtx = pStreamAudio->codec;
    AVCodec * pAudioDec = avcodec_find_decoder(m_pAudioDecCtx->codec_id);
    if (pAudioDec == NULL){
      avformat_close_input(&m_pFormatCtx);
      throw "Impossible de trouver le codec nécessaire";
    }
    
    //Initialise le codec
    if (avcodec_open2(m_pAudioDecCtx, pAudioDec, NULL)){
      avformat_close_input(&m_pFormatCtx);
      throw "Impossible d'initialiser le codec";
    }
    
    //Allocation de la frame et initialisation du packet recevant les données
    m_pCurrentFrame = av_frame_alloc();
    av_init_packet(&m_currentPacket);
    
    //TODO : allocation du buffer
    m_halt = false;
    m_pBufferFeeder = new thread(&LocalMusicSource::feedInternalBuffer, this);
  }
}

void LocalMusicSource::stop(){
  if (!m_halt){
    m_halt = true;

    //Libère le buffer
    delete[] m_aBuffer;
    //Libère la frame
    av_frame_free(&m_pCurrentFrame);  
    //Libère le packet traité actuellement
    av_free_packet(&m_currentPacket);
    //Libère le codec
    avcodec_close(m_pAudioDecCtx);
    //Ferme le fichier
    avformat_close_input(&m_pFormatCtx);

    //Fin du thread
    m_pBufferFeeder->join();
    delete m_pBufferFeeder;

  }
}


int LocalMusicSource::getData(uint8_t * data, const int maxData){
  //Erreur si aucune donnée n'est envoyée
  int dataSent = -1;

  if (m_availableData > 0){
    //Protection de la zone critique
    unique_lock<mutex> lock(m_mutex);
    
    //Si on a moins de donnée à envoyer que le max demandé
    if (m_availableData <= maxData){
      memcpy(data, m_aBuffer, m_availableData);
      dataSent = m_availableData;
      m_availableData = 0;    
    }
    //Si on a plus de donnée pretes que le max demandé
    else{
      memcpy(data, m_aBuffer, maxData);
      memmove(m_aBuffer, m_aBuffer + maxData, m_availableData - maxData);
      dataSent = maxData;
      m_availableData -= maxData;
    }

    //Fin de zone critique
    lock.unlock();
  }

  //Reste-t-il des données à envoyer ?
  m_halt = m_noPacketLeft && m_noFrameLeft && (m_availableData == 0);

  //Notifie m_pBufferFeeder pour qu'il puisse produire des données
  m_condVar.notify_one();

  return dataSent;
} 

int LocalMusicSource::fetchPacket(){
  //Libère l'ancien packet
  av_free_packet(&m_currentPacket);
  
  //Récupère le prochain packet audio
  int err = -1;
  do{
    err = av_read_frame (m_pFormatCtx, &m_currentPacket);
  } while(m_currentPacket.stream_index != m_pStreamAudio->index && !err);
  
  //Si err != 0, on a pas de packet valide
  
  if (err){
    if (err == EOF)
      m_noPacketLeft = true;
    return err;
  }

  //Copie du packet. Il est nécessaire de garder la copie originale du packet pour le libérer par la suite
  m_workingPacket = m_currentPacket;

  return 0;
}

int LocalMusicSource::fetchFrame(){
  int gotFrame = false;
  int err = -1;
  if (!m_noFrameLeft) {
    do {
      //Si le packet actuel est vide
      if (m_workingPacket.size == 0){
	//S'il n'y a plus aucun packet à récupérer
	if (m_noPacketLeft){
	  err = avcodec_decode_audio4(m_pAudioDecCtx, m_pCurrentFrame, &gotFrame, &m_workingPacket);
	  m_aFrameData = m_pCurrentFrame->extended_data;
	  m_unpaddedLinesize = m_pCurrentFrame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)m_pCurrentFrame->format);
	  if (err <= 0){
	    m_noFrameLeft = true;
	  }
	}
	//S'il reste des packets à récupérer
	else{
	  err = -1;
	  //Tant que je n'ai pas de frame à traiter & que tout va bien
	  do {
	    err = fetchPacket();
	    if (err) {
	      m_noFrameLeft = true;
	    }
	    //Si j'ai pu récupérer un packet
	    else {
	      err = -1;
	      do{
		int result = avcodec_decode_audio4(m_pAudioDecCtx, m_pCurrentFrame, &gotFrame, &m_workingPacket);
		m_aFrameData = m_pCurrentFrame->extended_data;
		m_unpaddedLinesize = m_pCurrentFrame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)m_pCurrentFrame->format);

		if (result >= 0){
		  m_workingPacket.data += result;
		  m_workingPacket.size -= result;
		}
		err = result;
	      } while((err > 0) && !gotFrame && (m_workingPacket.size > 0));
	    }
	  } while (!err && !gotFrame && !m_noPacketLeft);
	}
      }
    } while(!err && !gotFrame && !m_noFrameLeft);
  }

  return err;
}

void LocalMusicSource::processPlanarData(){
  
}

void LocalMusicSource::processNonPlanarData(){
  //Protection de la zone critique
  unique_lock<mutex> lock(m_mutex);
  
  while(!m_halt){
    m_condVar.wait(lock, [&](){return (m_availableData < m_bufferSize);});

    int freeSpace = m_bufferSize - m_availableData;
    while (freeSpace > 0 && !m_halt){
      if (m_unpaddedLinesize > freeSpace){
	memcpy(m_aBuffer, m_aFrameData, freeSpace);
	m_unpaddedLinesize -= freeSpace;
	m_aFrameData += freeSpace;

	m_availableData = m_bufferSize;
      }
      else {
	memcpy(m_aBuffer, m_aFrameData, m_unpaddedLinesize);
	m_availableData += m_unpaddedLinesize;

	fetchFrame();	
      }
      
      freeSpace = m_bufferSize - m_availableData;
    }
  }

  //Fin de zone critique
  lock.unlock();
}

void LocalMusicSource::feedInternalBuffer(){
  //TODO: gérer la conversion des formats
  //TODO: gérer le VBR
  
  bool isPlanar = av_sample_fmt_is_planar(m_pAudioDecCtx->sample_fmt);
  
  if (isPlanar){
    processPlanarData();
  }
  else{
    processNonPlanarData();
  }
}

bool LocalMusicSource::isFinished() const{
  return m_halt;
}

unsigned int LocalMusicSource::getSampleRate() const{
  return 0;
}
 
unsigned int LocalMusicSource::getChannelCount() const{
  return 0;
}
 
unsigned int LocalMusicSource::getSampleSize() const{
  return 0;
}
 
