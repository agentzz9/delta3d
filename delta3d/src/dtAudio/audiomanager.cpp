#include <cassert>
#include <stack>

#include <osg/Vec3>
#include <osg/io_utils>

#ifdef __APPLE__
  #include <OpenAL/alut.h>
#else
  #include <AL/alut.h>
#endif

#include <dtAudio/audiomanager.h>
#include <dtAudio/dtaudio.h>
#include <dtCore/system.h>
#include <dtCore/camera.h>
#include <dtCore/globals.h>
#include <dtUtil/stringutils.h>

// definitions
#if   !  defined(BIT)
#define  BIT(a)      (1L<<a)
#endif

// name spaces
using namespace   dtAudio;
using namespace   dtUtil;

AudioManager::MOB_ptr   AudioManager::_Mgr(NULL);
AudioManager::LOB_PTR   AudioManager::_Mic(NULL);
const char*             AudioManager::_EaxVer   = "EAX2.0";
const char*             AudioManager::_EaxSet   = "EAXSet";
const char*             AudioManager::_EaxGet   = "EAXGet";
const AudioConfigData   AudioManager::_DefCfg;

IMPLEMENT_MANAGEMENT_LAYER(AudioManager)

namespace dtAudio
{
////////////////////////////////////////////////////////////////////////////////
//Utitily function used to work with OpenAL's error messaging system.  It's used
//in multiple places throughout dtAudio.  It's not in AudioManager because we
//don't want things like Sound to directly access the AudioManager.
// Returns true on error, false if no error
bool CheckForError(const std::string& userMessage,
                   const std::string& msgFunction,
                   int lineNumber)
{
   ALenum error = alGetError();
   if (error != AL_NO_ERROR)
   {
      std::ostringstream finalStream;
      finalStream << "User Message: [" << userMessage << "] OpenAL Message: [" << alGetString(error) << "]";
      dtUtil::Log::GetInstance().LogMessage(dtUtil::Log::LOG_WARNING, msgFunction, lineNumber, finalStream.str().c_str());
      return AL_TRUE;
   }
   else
   {
      // check if we have an ALUT error
      ALenum alutError = alutGetError();
      if (alutError != ALUT_ERROR_NO_ERROR)
      {
         std::ostringstream finalStream;
         finalStream << "User Message: [" << userMessage << "] " << "Alut Message: [" << alutGetErrorString(alutError) << "] Line " << lineNumber;
         dtUtil::Log::GetInstance().LogMessage(dtUtil::Log::LOG_WARNING, msgFunction, lineNumber, finalStream.str().c_str());
         return AL_TRUE;
      }
   }
   return AL_FALSE;
}
} //namespace dtAudio

////////////////////////////////////////////////////////////////////////////////
// public member functions
// default consructor
AudioManager::AudioManager(const std::string& name /*= "audiomanager"*/)
   : Base(name)
   , mEAXSet(NULL)
   , mEAXGet(NULL)
   , mNumSources(0L)
   , mSource(NULL)
{
   RegisterInstance(this);

   AddSender(&dtCore::System::GetInstance());

   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   if (alutInit(NULL, NULL) == AL_FALSE)
   {
      std::cout << "Error initializing alut" << std::cout;
      CheckForError("alutInit(NULL, NULL)", __FUNCTION__, __LINE__);
   }
}

////////////////////////////////////////////////////////////////////////////////
// destructor
AudioManager::~AudioManager()
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   DeregisterInstance(this);

   try
   {
      // stop all sources
      if (mSource)
      {
         for (ALsizei ii(0); ii < mNumSources; ii++)
         {
            unsigned int sourceNum = mSource[ii];
            if (alIsSource(sourceNum))
            {
               alSourceStop(mSource[ii]);
               // This check was added to prevent a crash-on-exit for OSX -osb
               ALint bufValue;
               alGetSourcei(mSource[ii], AL_BUFFER, &bufValue);
               if (bufValue != 0)
               {
                  alSourcei(mSource[ii], AL_BUFFER, AL_NONE);
               }
            }
         }
         CheckForError("Source stopping and changing buffer to 0", __FUNCTION__, __LINE__);

         // delete the sources
         alDeleteSources(mNumSources, mSource);
         delete[] mSource;
         CheckForError("alDeleteSources(mNumSources, mSource);", __FUNCTION__, __LINE__);
      }

      mSourceMap.clear();
      mActiveList.clear();

      while (mAvailable.size())
      {
         mAvailable.pop();
      }

      while (mPlayQueue.size())
      {
         mPlayQueue.pop();
      }

      while (mPauseQueue.size())
      {
         mPauseQueue.pop();
      }

      while (mStopQueue.size())
      {
         mStopQueue.pop();
      }

      while (mRewindQueue.size())
      {
         mRewindQueue.pop();
      }

      // delete the buffers
      BufferData* bd(NULL);
      for (BUF_MAP::iterator iter(mBufferMap.begin()); iter != mBufferMap.end(); iter++)
      {
         if ((bd = iter->second) == NULL)
         {
            continue;
         }

         iter->second = NULL;
         alDeleteBuffers(1L, &bd->buf);
         delete bd;
         CheckForError("alDeleteBuffers( 1L, &bd->buf )", __FUNCTION__, __LINE__);
      }
      mBufferMap.clear();
      mSoundList.clear();

      while (mSoundCommand.size())
      {
         mSoundCommand.pop();
      }

      while (mSoundRecycle.size())
      {
         mSoundRecycle.pop();
      }

      alutExit();
      //CheckForError("alutExit()", __FUNCTION__, __LINE__);

      RemoveSender(&dtCore::System::GetInstance());
   }
   catch (...)
   {
      LOG_ERROR("Caught an exception of unknown type in the destructor of the AudioManager");
   }
}

////////////////////////////////////////////////////////////////////////////////
// create the singleton manager
void AudioManager::Instantiate(void)
{
   if (_Mgr.get())
   {
      return;
   }

   _Mgr  = new AudioManager;
   assert(_Mgr.get());

   _Mic  = new Listener;
   assert(_Mic.get());
}

////////////////////////////////////////////////////////////////////////////////
// destroy the singleton manager
void AudioManager::Destroy(void)
{
   _Mic = NULL;
   _Mgr = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// static instance accessor
AudioManager& AudioManager::GetInstance(void)
{
   return *_Mgr;
}


////////////////////////////////////////////////////////////////////////////////
// static listener accessor
Listener* AudioManager::GetListener(void)
{
   return static_cast<Listener*>(_Mic.get());
}

////////////////////////////////////////////////////////////////////////////////
bool AudioManager::GetListenerRelative(Sound* sound)
{
   //No sound?  Why are you asking about the ListenerRelative flag then?
   if(sound == NULL)
      return false;
   
   dtCore::RefPtr<dtAudio::Sound> snd = sound;
   ALuint src = -1;
   ALint srcRelativeFlag;

   //pull Sound object out of "the list"   
   std::vector<dtCore::RefPtr<dtAudio::Sound> >::iterator iter;
   for (iter = mSoundList.begin(); iter != mSoundList.end(); ++iter)
   {
      if (snd != *iter)
      {
         continue;
      }

      //found it
      src = (*iter)->GetSource();
      
      alGetSourcei(src, AL_SOURCE_RELATIVE, &srcRelativeFlag);

      if (srcRelativeFlag == 0)
         return false;
      else
         return true;
   }

   //couldn't find Sound in "the list"
   return false;     
}

////////////////////////////////////////////////////////////////////////////////
// manager configuration
void AudioManager::Config(const AudioConfigData& data /*= _DefCfg*/)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   if (mSource)
   {
      // already configured
      return;
   }

   // set up the distance model
   switch (data.distancemodel)
   {
   case AL_NONE:
      alDistanceModel(AL_NONE);
      break;

   case AL_INVERSE_DISTANCE_CLAMPED:
      alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
      break;

   case AL_INVERSE_DISTANCE:
   default:
      alDistanceModel(AL_INVERSE_DISTANCE);
      break;
   }

   CheckForError("alDistanceModel Changes", __FUNCTION__, __LINE__);

   // set up the sources
   if (!ConfigSources(data.numSources))
   {
      return;
   }

   CheckForError("ConfigSources( data.numSources )", __FUNCTION__, __LINE__);

   // set up EAX
   ConfigEAX(data.eax);
   CheckForError("ConfigEAX( data.eax )", __FUNCTION__, __LINE__);
}


////////////////////////////////////////////////////////////////////////////////
// message receiver
void AudioManager::OnMessage(MessageData* data)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert(data);

   if (data->sender == &dtCore::System::GetInstance())
   {
      // system messages
      if (data->message == dtCore::System::MESSAGE_PRE_FRAME)
      {
         PreFrame(*static_cast<const double*>(data->userData));
         return;
      }

      if (data->message == dtCore::System::MESSAGE_FRAME)
      {
         Frame(*static_cast<const double*>(data->userData));
         return;
      }

      if (data->message == dtCore::System::MESSAGE_POST_FRAME)
      {
         PostFrame(*static_cast<const double*>(data->userData));
         return;
      }

      if (data->message == dtCore::System::MESSAGE_PAUSE)
      {
         // During a system-wide pause, we want the AudioManager to behave
         // as normal. In many games, there are sounds that occur during
         // during a pause, such as background music or GUI clicks. So
         // here we just call the normal functions all at once.
         PreFrame(*static_cast<const double*>(data->userData));
         Frame(*static_cast<const double*>(data->userData));
         PostFrame(*static_cast<const double*>(data->userData));
         return;
      }

      if (data->message == dtCore::System::MESSAGE_PAUSE_START)
      {
         mSoundStateMap.clear();

         // Pause all sounds that are currently playing, and
         // save their previous state.
         for (SND_LST::iterator iter = mSoundList.begin(); iter != mSoundList.end(); iter++)
         {
            Sound* sob = iter->get();

            if (sob->IsPaused())
            {
               mSoundStateMap.insert(SoundObjectStateMap::value_type(sob, PAUSED));
            }
            else if (sob->IsPlaying())
            {
               mSoundStateMap.insert(SoundObjectStateMap::value_type(sob, PLAYING));
            }
            else if (sob->IsStopped())
            {
               mSoundStateMap.insert(SoundObjectStateMap::value_type(sob, STOPPED));
            }

            PauseSound(sob);
         }
      }

      if (data->message == dtCore::System::MESSAGE_PAUSE_END)
      {
         // Restore all paused sounds to their previous state.
         for (SND_LST::iterator iter = mSoundList.begin(); iter != mSoundList.end(); iter++)
         {
            Sound* sob = iter->get();

            switch (mSoundStateMap[sob])
            {
            case PAUSED:
               PauseSound( sob );
               break;
            case PLAYING:
               PlaySound( sob );
               break;
            case STOPPED:
               StopSound( sob );
               break;
            default:
               break;
            }
         }
      }
   }
   else
   {
      Sound::Command sndCommand = Sound::NONE;

      // sound commands
      if(data->message == Sound::kCommand[Sound::POSITION])
      {
         sndCommand = Sound::POSITION;
      }
      else if(data->message == Sound::kCommand[Sound::DIRECTION])
      {
         sndCommand = Sound::DIRECTION;
      }
      else if(data->message == Sound::kCommand[Sound::VELOCITY])
      {
         sndCommand = Sound::VELOCITY;
      }
      else if(data->message == Sound::kCommand[Sound::PLAY])
      {
         sndCommand = Sound::PLAY;
      }
      else if(data->message == Sound::kCommand[Sound::STOP])
      {
         sndCommand = Sound::STOP;
      }
      else if(data->message == Sound::kCommand[Sound::PAUSE])
      {
         sndCommand = Sound::PAUSE;
      }
      else if(data->message == Sound::kCommand[Sound::LOAD])
      {
         sndCommand = Sound::LOAD;
      }
      else if(data->message == Sound::kCommand[Sound::UNLOAD])
      {
         sndCommand = Sound::UNLOAD;
      }
      else if(data->message == Sound::kCommand[Sound::LOOP])
      {
         sndCommand = Sound::LOOP;
      }
      else if(data->message == Sound::kCommand[Sound::UNLOOP])
      {
         sndCommand = Sound::UNLOOP;
      }
      else if(data->message == Sound::kCommand[Sound::GAIN])
      {
         sndCommand = Sound::GAIN;
      }
      else if(data->message == Sound::kCommand[Sound::PITCH])
      {
         sndCommand = Sound::PITCH;
      }
      else if(data->message == Sound::kCommand[Sound::REWIND])
      {
         sndCommand = Sound::REWIND;
      }
      else if(data->message == Sound::kCommand[Sound::REL])
      {
         sndCommand = Sound::REL;
      }
      else if(data->message == Sound::kCommand[Sound::ABS])
      {
         sndCommand = Sound::ABS;
      }
      else if(data->message == Sound::kCommand[Sound::MIN_DIST])
      {
         sndCommand = Sound::MIN_DIST;
      }
      else if(data->message == Sound::kCommand[Sound::MAX_DIST])
      {
         sndCommand = Sound::MAX_DIST;
      }
      else if(data->message == Sound::kCommand[Sound::ROL_FACT])
      {
         sndCommand = Sound::ROL_FACT;
      }
      else if(data->message == Sound::kCommand[Sound::MIN_GAIN])
      {
         sndCommand = Sound::MIN_GAIN;
      }
      else if(data->message == Sound::kCommand[Sound::MAX_GAIN])
      {
         sndCommand = Sound::MAX_GAIN;
      }

      if( sndCommand != Sound::NONE )
      {
         assert(data->userData);
         Sound* snd(static_cast<Sound*>(data->userData));
         snd->EnqueueCommand(Sound::kCommand[sndCommand]);
         mSoundCommand.push(snd);
      }
   }
   CheckForError("Something went wrong in the OnMessage Method", __FUNCTION__, __LINE__);
}


////////////////////////////////////////////////////////////////////////////////
Sound* AudioManager::NewSound(void)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   SOB_PTR snd(NULL);

   // first look if we can recycle a sound
   if (!mSoundRecycle.empty())
   {
      snd = mSoundRecycle.front();
      assert(snd.get());

      snd->Clear();
      mSoundRecycle.pop();
   }

   // create a new sound object if we don't have one
   if (snd.get() == NULL)
   {
      snd = new Sound();
      assert(snd.get());
   }

   // listen to messages from this guy
   AddSender(snd.get());

   // save the sound
   mSoundList.push_back(snd);

   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);

   // hand out the interface to the sound
   return static_cast<Sound*>(snd.get());
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::FreeSound(Sound* sound)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   SOB_PTR snd = static_cast<Sound*>(sound);

   if(snd.get() == NULL)
   {
      return;
   }

   // remove sound from list
   SND_LST::iterator iter;
   for (iter = mSoundList.begin(); iter != mSoundList.end(); iter++)
   {
      if (snd != *iter)
      {
         continue;
      }

      mSoundList.erase(iter);
      break;
   }

   // stop listening to this guys messages
   snd->RemoveSender(this);
   snd->RemoveSender(&dtCore::System::GetInstance());
   RemoveSender(snd.get());

   // free the sound's source and buffer
   FreeSource(snd.get());
   UnloadSound(snd.get());
   snd->Clear();

   // recycle this sound
   mSoundRecycle.push(snd);
}

////////////////////////////////////////////////////////////////////////////////
ALuint AudioManager::GetSource(Sound* sound)
{
   dtCore::RefPtr<dtAudio::Sound> snd = static_cast<Sound*>(sound);
   ALuint src = -1;

   //pull Sound out of "the list"   
   std::vector<dtCore::RefPtr<dtAudio::Sound> >::iterator iter;
   for (iter = mSoundList.begin(); iter != mSoundList.end(); ++iter)
   {
      if (snd != *iter)
      {
         continue;
      }

      src = (*iter)->GetSource();
      break;
   }

   return src;
}

////////////////////////////////////////////////////////////////////////////////
bool AudioManager::LoadFile(const std::string& file)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   if (file.empty())
   {
      // no file name, bail...
      return false;
   }

   std::string filename = dtCore::FindFileInPathList(file);
   if (filename.empty())
   {
      // still no file name, bail...
      Log::GetInstance().LogMessage(Log::LOG_WARNING, __FUNCTION__, "AudioManager: can't load file %s", file.c_str());
      return false;
   }

   BufferData* bd = mBufferMap[file];
   if (bd != 0)
   {
      // file already loaded, bail...
      return false;
   }

   bd = new BufferData;

   // Clear the errors
   //ALenum err( alGetError() );

   // create buffer for the wave file
   alGenBuffers(1L, &bd->buf);
   if (CheckForError("AudioManager: alGenBuffers error", __FUNCTION__, __LINE__))
   {
      delete bd;
      return false;
   }

   ALenum format(0);
   ALsizei size(0);
   ALvoid* data = NULL;

   // We are trying to support the new version of ALUT as well as the old intergated
   // version. So we have two cases: DEPRECATED and NON-DEPRECATED.

   // This is not defined in ALUT prior to version 1.
   #ifndef ALUT_API_MAJOR_VERSION

   // DEPRECATED version for ALUT < 1.0.0

   // Man, are we still in the dark ages here???
   // Copy the std::string to a frickin' ALByte array...
   ALbyte fname[256L];
   unsigned int len = std::min(filename.size(), size_t(255L));
   memcpy(fname, filename.c_str(), len);
   fname[len] = 0L;

   ALsizei freq(0);
   #ifdef __APPLE__
   alutLoadWAVFile(fname, &format, &data, &size, &freq);
   #else
   alutLoadWAVFile(fname, &format, &data, &size, &freq, &bd->loop);
   #endif // __APPLE__

   #else

   // NON-DEPRECATED version for ALUT >= 1.0.0
   ALfloat freq(0);
   data = alutLoadMemoryFromFile(filename.c_str(), &format, &size, &freq);
   CheckForError("data = alutLoadMemoryFromFile", __FUNCTION__, __LINE__);

   #endif // ALUT_API_MAJOR_VERSION

   if (data == NULL)
   {
      #ifndef ALUT_API_MAJOR_VERSION
      Log::GetInstance().LogMessage(Log::LOG_WARNING, __FUNCTION__,
         "AudioManager: alutLoadWAVFile error on %s", file.c_str());
      #else
         CheckForError("AudioManager: alutLoadMemoryFromFile error", __FUNCTION__, __LINE__);
      #endif // ALUT_API_MAJOR_VERSION

      alDeleteBuffers(1L, &bd->buf);
      delete bd;
      CheckForError("alDeleteBuffers error", __FUNCTION__, __LINE__);
      return false;
   }

   alBufferData(bd->buf, format, data, size, ALsizei(freq));
   if (CheckForError("AudioManager: alBufferData error ", __FUNCTION__, __LINE__))
   {
      alDeleteBuffers(1L, &bd->buf);
      free(data);

      delete bd;
      return false;
   }

   // The ALUT documentation says you are "free" to free the allocated
   // memory after it has been copied to the OpenAL buffer. See:
   // http://www.openal.org/openal_webstf/specs/alut.html#alutLoadMemoryFromFile
   // This works fine on Linux, but crashes with a heap error on Windows.
   // This is probably a Windows implementation bug, so let's just leak a
   // bit in the meantime. Hope you bought your Timberlands...
   // -osb

   #if !defined (WIN32) && !defined (_WIN32) && !defined (__WIN32__)
      free(data);
   #endif

   mBufferMap[file] = bd;
   bd->file = mBufferMap.find(file)->first.c_str();

   return true;
}

////////////////////////////////////////////////////////////////////////////////
bool AudioManager::UnloadFile(const std::string& file)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   if (file.empty())
   {
      // no file name, bail...
      return false;
   }

   BUF_MAP::iterator iter = mBufferMap.find(file);
   if (iter == mBufferMap.end())
   {
      // file is not loaded, bail...
      return false;
   }

   BufferData* bd = iter->second;
   if (bd == 0)
   {
      // bd should never be NULL
      // this code should never run
      mBufferMap.erase(iter);
      return false;
   }

   if (bd->use)
   {
      // buffer still in use, don't remove buffer
      return false;
   }

   alDeleteBuffers(1L, &bd->buf);
   delete bd;
   CheckForError("alDeleteBuffers( 1L, &bd->buf );", __FUNCTION__, __LINE__);

   mBufferMap.erase(iter);
   return true;
}



////////////////////////////////////////////////////////////////////////////////
// private member functions
void AudioManager::PreFrame(const double deltaFrameTime)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   SOB_PTR     snd(NULL);
   const char* cmd(NULL);

   // flush all the sound commands
   while (mSoundCommand.size())
   {
      snd = mSoundCommand.front();
      mSoundCommand.pop();

      if ( snd.get() == NULL )
         continue;

      cmd   = snd->DequeueCommand();

      // set sound position
      if ( cmd == Sound::kCommand[Sound::POSITION] )
      {
         SetPosition( snd.get() );
         continue;
      }

      // set sound direction
      if ( cmd == Sound::kCommand[Sound::DIRECTION] )
      {
         SetDirection( snd.get() );
         continue;
      }

      // set sound velocity
      if ( cmd == Sound::kCommand[Sound::VELOCITY] )
      {
         SetVelocity( snd.get() );
         continue;
      }

      // set sound to play
      if ( cmd == Sound::kCommand[Sound::PLAY] )
      {
         PlaySound( snd.get() );
         continue;
      }

      // set sound to stop
      if ( cmd == Sound::kCommand[Sound::STOP] )
      {
         StopSound( snd.get() );
         continue;
      }

      // set sound to pause
      if ( cmd == Sound::kCommand[Sound::PAUSE] )
      {
         PauseSound( snd.get() );
         continue;
      }

      // loading a new sound
      if ( cmd == Sound::kCommand[Sound::LOAD] )
      {
         LoadSound( snd.get() );
         continue;
      }

      // unloading an old sound
      if ( cmd == Sound::kCommand[Sound::UNLOAD] )
      {
         UnloadSound( snd.get() );
         continue;
      }

      // setting the loop flag
      if ( cmd == Sound::kCommand[Sound::LOOP] )
      {
         SetLoop( snd.get() );
         continue;
      }

      // un-setting the loop flag
      if ( cmd == Sound::kCommand[Sound::UNLOOP] )
      {
         ResetLoop( snd.get() );
         continue;
      }

      // setting the gain
      if ( cmd == Sound::kCommand[Sound::GAIN] )
      {
         SetGain( snd.get() );
         continue;
      }

      // setting the pitch
      if ( cmd == Sound::kCommand[Sound::PITCH] )
      {
         SetPitch( snd.get() );
         continue;
      }

      // rewind the sound
      if ( cmd == Sound::kCommand[Sound::REWIND] )
      {
         RewindSound( snd.get() );
         continue;
      }

      // set sound relative to listener
      if ( cmd == Sound::kCommand[Sound::REL] )
      {
         SetRelative( snd.get() );
         continue;
      }

      // set sound absolute (not relative to listener)
      if ( cmd == Sound::kCommand[Sound::ABS] )
      {
         SetAbsolute( snd.get() );
         continue;
      }

      // set minimum distance for attenuation
      if ( cmd == Sound::kCommand[Sound::MIN_DIST] )
      {
         SetReferenceDistance( snd.get() );
         continue;
      }

      // set maximum distance for attenuation
      if ( cmd == Sound::kCommand[Sound::MAX_DIST] )
      {
         SetMaximumDistance( snd.get() );
         continue;
      }

      // set minimum gain for attenuation
      if ( cmd == Sound::kCommand[Sound::MIN_GAIN] )
      {
         SetMinimumGain( snd.get() );
         continue;
      }

      // set maximum gain for attenuation
      if ( cmd == Sound::kCommand[Sound::MAX_GAIN] )
      {
         SetMaximumGain( snd.get() );
         continue;
      }

      // set the roll off attenutation factor
      if ( cmd == Sound::kCommand[Sound::ROL_FACT] )
      {
         SetRolloff( snd.get() );
         continue;
      }
   }
   CheckForError("PreFrame Error checking", __FUNCTION__, __LINE__);
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::Frame( const double deltaFrameTime )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   SRC_LST::iterator             iter;
   std::stack<SRC_LST::iterator> stk;
   ALuint                        src(0L);
   ALint                         state(AL_STOPPED);
   SOB_PTR                       snd(NULL);

   // signal any sources commanded to stop
   while ( mStopQueue.size() )
   {
      src   = mStopQueue.front();
      mStopQueue.pop();

      assert( alIsSource( src ) );

      alSourceStop( src );
   }

   CheckForError("Processing stop queue error", __FUNCTION__, __LINE__);

   // push the new sources onto the active list
   while ( mPlayQueue.size() )
   {
      src   = mPlayQueue.front();
      mPlayQueue.pop();

      assert( alIsSource( src ) );

      mActiveList.push_back( src );
   }
   CheckForError("alIsSource error", __FUNCTION__, __LINE__);

   // start any new sounds and
   // remove any sounds that have stopped
   for ( iter = mActiveList.begin(); iter != mActiveList.end(); iter++ )
   {
      src   = *iter;
      assert( alIsSource( src ) == AL_TRUE );

      alGetSourcei( src, AL_SOURCE_STATE, &state );
      if (CheckForError("AudioManager: alGetSourcei(AL_SOURCE_STATE) error", __FUNCTION__, __LINE__))
         continue;

      switch( state )
      {
         case  AL_PLAYING:
         case  AL_PAUSED:
            // don't need to do anything
            break;

         case  AL_INITIAL:
            {
               // start any new sources
               alSourcePlay( src );

               // send play message
               snd   = mSourceMap[src];
               if ( snd.get() )
               {
                  SendMessage( Sound::kCommand[Sound::PLAY], snd.get() );
               }
            }
            break;

         case  AL_STOPPED:
            {
               // send stopped message
               snd   = mSourceMap[src];
               if ( snd.get() )
               {
                  SendMessage( Sound::kCommand[Sound::STOP], snd.get() );
               }

               // save stopped sound iterator for later removal
               stk.push( iter );
            }
            break;

         default:
            break;
      }
   }


   // signal any sources commanded to pause
   while ( mPauseQueue.size() )
   {
      src   = mPauseQueue.front();
      mPauseQueue.pop();

      assert( alIsSource( src ) );

      alGetSourcei( src, AL_SOURCE_STATE, &state );
      if (CheckForError("AudioManager: alGetSourcei(AL_SOURCE_STATE) error", __FUNCTION__, __LINE__))
         continue;

      switch( state )
      {
         case  AL_PLAYING:
            {
               alSourcePause( src );

               // send pause message
               snd   = mSourceMap[src];
               if ( snd.get() )
               {
                  SendMessage( Sound::kCommand[Sound::PAUSE], snd.get() );
               }
            }
            break;

         case  AL_PAUSED:
            {
               alSourcePlay( src );

               // send pause message
               snd   = mSourceMap[src];
               if ( snd.get() )
               {
                  SendMessage( Sound::kCommand[Sound::PLAY], snd.get() );
               }
            }
            break;

         default:
            break;
      }
   }


   // signal any sources commanded to rewind
   while ( mRewindQueue.size() )
   {
      src   = mRewindQueue.front();
      mRewindQueue.pop();

      assert( alIsSource( src ) );

      alSourceRewind( src );

      // send rewind message
      snd   = mSourceMap[src];
      if ( snd.get() )
      {
         SendMessage( Sound::kCommand[Sound::REWIND], snd.get() );
      }
   }

   CheckForError("alSourceRewind", __FUNCTION__, __LINE__);

   // remove stopped sounds from the active list
   while ( stk.size() )
   {
      iter  = stk.top();
      stk.pop();

      src   = *iter;
      mStopQueue.push( src );
      mActiveList.erase( iter );
   }
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::PostFrame( const double deltaFrameTime )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   SOB_PTR     snd(NULL);
   ALuint      src(0L);

   // for all sounds that have stopped
   while ( mStopQueue.size() )
   {
      // free the source for later use
      src   = mStopQueue.front();
      mStopQueue.pop();

      snd   = mSourceMap[src];
      if (snd.valid())
      {
         snd->RemoveSender( this );
         snd->RemoveSender( &dtCore::System::GetInstance() );

         FreeSource( snd.get() );
      }
   }
   CheckForError("Post Frame Error", __FUNCTION__, __LINE__);
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::Pause( const double deltaFrameTime )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   SRC_LST::iterator             iter;
   std::stack<SRC_LST::iterator> stk;
   ALuint                        src(0L);
   ALint                         state(AL_STOPPED);
   SOB_PTR                       snd(NULL);

   // signal any sources commanded to stop
   while ( mStopQueue.size() )
   {
      src   = mStopQueue.front();
      mStopQueue.pop();

      assert( alIsSource( src ) );

      alSourceStop( src );
   }

   CheckForError("Source stop error", __FUNCTION__, __LINE__);

   // start any new sounds and
   // remove any sounds that have stopped
   for ( iter = mActiveList.begin(); iter != mActiveList.end(); iter++ )
   {
      src   = *iter;
      assert( alIsSource( src ) == AL_TRUE );

      alGetSourcei( src, AL_SOURCE_STATE, &state );

      if (CheckForError("AudioManager: alGetSourcei(AL_SOURCE_STATE) error", __FUNCTION__, __LINE__))
         continue;

      switch( state )
      {
         case  AL_PLAYING:
         case  AL_PAUSED:
            // don't need to do anything
            break;
         case  AL_STOPPED:
            {
               // send stopped message
               snd   = mSourceMap[src];
               if ( snd.get() )
               {
                  SendMessage( Sound::kCommand[Sound::STOP], snd.get() );
               }

               // save stopped sound iterator for later removal
               stk.push( iter );
            }
            break;

         default:
            break;
      }
   }

   // signal any sources commanded to pause
   while ( mPauseQueue.size() )
   {
      src   = mPauseQueue.front();
      mPauseQueue.pop();

      assert( alIsSource( src ) );

      alGetSourcei( src, AL_SOURCE_STATE, &state );
      if (CheckForError("AudioManager: alGetSourcei(AL_SOURCE_STATE) error", __FUNCTION__, __LINE__))
         continue;

      switch( state )
      {
         case  AL_PLAYING:
            {
               alSourcePause( src );

               // send pause message
               snd   = mSourceMap[src];
               if ( snd.get() )
               {
                  SendMessage( Sound::kCommand[Sound::PAUSE], snd.get() );
               }
            }
            break;

         case  AL_PAUSED:
            {
               alSourcePlay( src );

               // send pause message
               snd   = mSourceMap[src];
               if ( snd.get() )
               {
                  SendMessage( Sound::kCommand[Sound::PLAY], snd.get() );
               }
            }
            break;

         default:
            break;
      }
   }

   // signal any sources commanded to rewind
   while ( mRewindQueue.size() )
   {
      src   = mRewindQueue.front();
      mRewindQueue.pop();

      assert( alIsSource( src ) );

      alSourceRewind( src );

      // send rewind message
      snd   = mSourceMap[src];
      if ( snd.get() )
      {
         SendMessage( Sound::kCommand[Sound::REWIND], snd.get() );
      }
   }

   CheckForError("alSourceRewind error", __FUNCTION__, __LINE__);

   LOGN_ALWAYS("audiomanager.cpp", "paused5");
   // remove stopped sounds from the active list
   while ( stk.size() )
   {
      iter  = stk.top();
      stk.pop();

      src   = *iter;
      mStopQueue.push( src );
      mActiveList.erase( iter );
   }
}

////////////////////////////////////////////////////////////////////////////////
bool AudioManager::Configured( void ) const
{
   return   mSource != NULL;
}

////////////////////////////////////////////////////////////////////////////////
bool AudioManager::ConfigSources( unsigned int num )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   if ( num == 0L )
      return   false;

   mNumSources = ALsizei(num);

   mSource  = new ALuint[mNumSources];
   if (mSource == NULL)
   {
      LOG_ERROR("Failed allocating " + dtUtil::ToString(mNumSources) + " sources.");
      return false;
   }

   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   alGenSources( mNumSources, mSource );
   if (CheckForError("AudioManager: alGenSources Error", __FUNCTION__, __LINE__))
   {
      if ( mSource )
      {
         delete   mSource;
         mSource  = NULL;
      }
      return   false;
   }

   for ( ALsizei ii(0); ii < mNumSources; ii++ )
   {
      if (alIsSource( mSource[ii] ) == AL_FALSE)
      {
         LOG_ERROR("Source " + dtUtil::ToString(ii) + " is not valid.");
         return false;
      }
      mAvailable.push( mSource[ii] );
   }

   return   true;
}



////////////////////////////////////////////////////////////////////////////////
bool AudioManager::ConfigEAX( bool eax )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   if ( !eax )
   {
      return false;
   }

   #ifndef AL_VERSION_1_1
   ALubyte buf[32L];
   memset( buf, 0L, 32L );
   memcpy( buf, _EaxVer, std::min( strlen(_EaxVer), size_t(32L) ) );
   #else
   const ALchar* buf = _EaxVer;
   #endif

   // check for EAX support
   if ( alIsExtensionPresent(buf) == AL_FALSE )
   {
      Log::GetInstance().LogMessage( Log::LOG_WARNING, __FUNCTION__,
         "AudioManager: %s is not available", _EaxVer );
      return   false;
   }

   #ifndef AL_VERSION_1_1
   memset( buf, 0L, 32L );
   memcpy( buf, _EaxSet, std::min( strlen(_EaxSet), size_t(32L) ) );
   #else
   buf = _EaxSet;
   #endif

   // get the eax-set function
   mEAXSet = alGetProcAddress(buf);
   if ( mEAXSet == 0 )
   {
      Log::GetInstance().LogMessage( Log::LOG_WARNING, __FUNCTION__,
         "AudioManager: %s is not available", _EaxVer );
      return   false;
   }

   #ifndef AL_VERSION_1_1
   memset( buf, 0, 32 );
   memcpy( buf, _EaxGet, std::min( strlen(_EaxGet), size_t(32) ) );
   #else
   buf = _EaxVer;
   #endif

   // get the eax-get function
   mEAXGet = alGetProcAddress(buf);
   if ( mEAXGet == 0 )
   {
      Log::GetInstance().LogMessage( Log::LOG_WARNING, __FUNCTION__,
         "AudioManager: %s is not available", _EaxVer );
      mEAXSet = 0;
      return false;
   }

   CheckForError("Config eax issue", __FUNCTION__, __LINE__);

   return true;
}



////////////////////////////////////////////////////////////////////////////////
void AudioManager::LoadSound( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   const char* file(static_cast<Sound*>(snd)->GetFilename());

   if ( file == NULL )
      return;

   LoadFile( file );

   BufferData* bd = mBufferMap[file];

   if ( bd == NULL )
      return;

   bd->use++;
   snd->SetBuffer(bd->buf);
   CheckForError("LoadSound Error", __FUNCTION__, __LINE__);
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::UnloadSound( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   const char* file(static_cast<Sound*>(snd)->GetFilename());

   if ( file == NULL )
   {
      return;
   }

   snd->SetBuffer(0);

   BufferData* bd = mBufferMap[file];

   if ( bd == NULL )
   {
      return;
   }

   bd->use--;

   // If the buffer is not being used by any other sound object...
   if( bd->use == 0 )
   {
      ALuint src = snd->GetSource();
      if ( alIsSource( src ) && snd->IsInitialized() )
      {
         // ...free the source before attempting to unload
         // the sound file, which will subsequently attempt
         // to delete the buffer. This will ensure the sound
         // source is properly stopped and reset before the
         // sound buffer is deleted.
         FreeSource( snd );

         // NOTE: Stop commands for the sound will not be
         // processed until the next call to Frame. This is
         // VERY bad if the sound is told to stop and unload on
         // on the same frame; in this case the sound will not
         // be stopped when it is attempted to be unloaded.
         //
         // NOTE: Deleting the buffer will fail if the sound
         // source is still playing and thus result in a very
         // sound memory leak and potentially mess up the
         // the use of the sources for sounds; sources that
         // should have been freed will essentially become locked.
      }
   }

   UnloadFile( file );
   CheckForError("Unload Sound Error", __FUNCTION__, __LINE__);
}



////////////////////////////////////////////////////////////////////////////////
void AudioManager::PlaySound(Sound* snd)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   ALuint buf(0);
   ALuint src(0);
   bool source_is_new = false;

   assert(snd);

   // first check if sound has a buffer
   buf = snd->GetBuffer();
   if(alIsBuffer(buf) == AL_FALSE)
   {
      CheckForError("alIsBuffer error", __FUNCTION__, __LINE__);
      // no buffer, bail
      return;
   }

   // then check if sound has a source
   src = snd->GetSource();
   if(!snd->IsInitialized())
   {
      // no source, gotta get one
      if(!SetupSource(snd))
      {
         // no source available
         Log::GetInstance().LogMessage( Log::LOG_ERROR, __FUNCTION__,
            "AudioManager: play attempt w/o available sources" );
         Log::GetInstance().LogMessage( Log::LOG_ERROR, __FUNCTION__,
            "AudioManager: try increasing the number of sources at config time" );
         return;
      }

      src = snd->GetSource();
      source_is_new = true;
   }
   else
   {
      // already has buffer and source
      // could be paused (or playing)
      ALint state(AL_STOPPED);
      alGetSourcei(src, AL_SOURCE_STATE, &state);

      if(CheckForError("AudioManager: alGetSourcei(AL_SOURCE_STATE) error", __FUNCTION__, __LINE__))
      {
         return;
      }

      switch(state)
      {
         case AL_PAUSED:
            mPauseQueue.push(src);
            // no break, run to next case

         case AL_PLAYING:
            return;
            break;

         default:
            // either initialized or stopped
            // continue setting the buffer and playing
            break;
      }
   }

   // bind the buffer to the source
   alSourcei(src, AL_BUFFER, buf);
   if (CheckForError("AudioManager: alSourcei(AL_BUFFER) error", __FUNCTION__, __LINE__))
   {
      return;
   }

   // set looping flag
   alSourcei(src, AL_LOOPING, (snd->IsLooping()) ? AL_TRUE : AL_FALSE);
   CheckForError("AudioManager: alSourcei(AL_LOOPING) error", __FUNCTION__, __LINE__);

   // set source relative flag
   if(snd->IsListenerRelative())
   {
      // is listener relative
      alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);      
      CheckForError("AudioManager: alSourcei(AL_SOURCE_RELATIVE) error", __FUNCTION__, __LINE__);

      // set initial position and direction
      osg::Vec3 pos;
      osg::Vec3 dir;

      snd->GetPosition(pos);
      snd->GetDirection(dir);

      alSource3f(src,
         AL_POSITION,
         static_cast<ALfloat>(pos[0]),
         static_cast<ALfloat>(pos[1]),
         static_cast<ALfloat>(pos[2]));

      CheckForError("AudioManager: alSource3f(AL_POSITION) error", __FUNCTION__, __LINE__);

      alSource3f(src,
         AL_DIRECTION,
         static_cast<ALfloat>(dir[0]),
         static_cast<ALfloat>(dir[1]),
         static_cast<ALfloat>(dir[2]));
      CheckForError("AudioManager: alSource3f(AL_DIRECTION) error", __FUNCTION__, __LINE__);
   }
   else
   {
      // not listener relative
      alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);      
      CheckForError("AudioManager: alSourcei(AL_SOURCE_RELATIVE) error", __FUNCTION__, __LINE__);
   }

   // set gain
   alSourcef(src, AL_GAIN, static_cast<ALfloat>(snd->GetGain()));
   CheckForError("AudioManager: alSourcef(AL_GAIN) error", __FUNCTION__, __LINE__);

   // set pitch
   alSourcef(src, AL_PITCH, static_cast<ALfloat>(snd->GetPitch()));
   CheckForError("AudioManager: alSourcef(AL_PITCH) error", __FUNCTION__, __LINE__);

   // set reference distance
   if(snd->GetState(Sound::MIN_DIST) || source_is_new)
   {
      snd->ResetState(Sound::MIN_DIST);
      alSourcef(src, AL_REFERENCE_DISTANCE, static_cast<ALfloat>(snd->GetMinDistance()));
      CheckForError("AudioManager: alSourcef(AL_REFERENCE_DISTANCE) error", __FUNCTION__, __LINE__);
   }

   // set maximum distance
   if(snd->GetState(Sound::MAX_DIST) || source_is_new)
   {
      snd->ResetState(Sound::MAX_DIST);
      alSourcef(src, AL_MAX_DISTANCE, static_cast<ALfloat>(snd->GetMaxDistance()));
      CheckForError("AudioManager: alSourcef(AL_MAX_DISTANCE) error", __FUNCTION__, __LINE__);
   }

   // set rolloff factor
   if(snd->GetState(Sound::ROL_FACT) || source_is_new)
   {
      snd->ResetState(Sound::ROL_FACT);
      alSourcef(src, AL_ROLLOFF_FACTOR, static_cast<ALfloat>(snd->GetRolloffFactor()));
      CheckForError("AudioManager: alSourcef(AL_ROLLOFF_FACTOR) error", __FUNCTION__, __LINE__);
   }

   // set minimum gain
   if(snd->GetState(Sound::MIN_GAIN ) || source_is_new)
   {
      snd->ResetState(Sound::MIN_GAIN);
      alSourcef(src, AL_MIN_GAIN, static_cast<ALfloat>(snd->GetMinGain()));
      CheckForError("AudioManager: alSourcef(AL_MIN_GAIN) error", __FUNCTION__, __LINE__);
   }

   // set maximum gain
   if(snd->GetState( Sound::MAX_GAIN ) || source_is_new)
   {
      snd->ResetState(Sound::MAX_GAIN);
      alSourcef(src, AL_MAX_GAIN, static_cast<ALfloat>(snd->GetMaxGain()));
      CheckForError("AudioManager: alSourcef(AL_MAX_GAIN) error", __FUNCTION__, __LINE__);
   }

   if(source_is_new)
   {
      snd->AddSender(this );
      snd->AddSender(&dtCore::System::GetInstance());
      mPlayQueue.push(snd->GetSource());
   }
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::PauseSound( Sound* snd )
{
   assert( snd );

   if ( !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }

   mPauseQueue.push( snd->GetSource() );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::StopSound( Sound* snd )
{
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }

   mStopQueue.push( src );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::RewindSound( Sound* snd )
{
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }

   mRewindQueue.push( src );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetLoop( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE  || !snd->IsInitialized() )
   {
      // sound is not playing
      // set flag and bail
      snd->SetState( Sound::LOOP );
      return;
   }


   alSourcei( src, AL_LOOPING, AL_TRUE );
   if (CheckForError("AudioManager: alSourcei(AL_LOOP) error", __FUNCTION__, __LINE__))
      return;

   SendMessage( Sound::kCommand[Sound::LOOP], snd );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::ResetLoop( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing
      // set flag and bail
      snd->ResetState( Sound::LOOP );
      return;
   }


   alSourcei( src, AL_LOOPING, AL_FALSE );
   if (CheckForError("AudioManager: alSourcei(AL_LOOP) error %X", __FUNCTION__, __LINE__))
      return;

   SendMessage( Sound::kCommand[Sound::UNLOOP], snd );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetRelative(Sound* snd)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert(snd);

   if(snd->IsListenerRelative())
   {
      // already set, bail
      return;
   }

   ALuint buf = snd->GetBuffer();
   if(alIsBuffer(buf) == AL_FALSE)
   {
      // does not have sound buffer
      // set flag and bail
      snd->SetState(Sound::REL);
      snd->ResetState(Sound::ABS);
      return;
   }
   else
   {
      // check for stereo
      // multiple channels don't get positioned
      ALint numchannels(0L);
      alGetBufferi(buf, AL_CHANNELS, &numchannels);
      if(numchannels == 2L)
      {
         // stereo!
         // set flag and bail
         Log::GetInstance().LogMessage(Log::LOG_INFO, __FUNCTION__,
            "AudioManager: A stereo Sound can't be positioned in 3D space");
         snd->SetState(Sound::REL);
         snd->ResetState(Sound::ABS);
         return;
      }
   }

   ALuint src = snd->GetSource();
   if(alIsSource(src) == AL_FALSE || !snd->IsInitialized())
   {
      // sound is not playing
      // set flag and bail
      snd->SetState(Sound::REL);
      snd->ResetState(Sound::ABS);
      return;
   }

   CheckForError("alGetBufferi && alIsSource calls check", __FUNCTION__, __LINE__);
   alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);   
   if (CheckForError("AudioManager: alSourcei(AL_SOURCE_RELATIVE) error", __FUNCTION__, __LINE__))
   {
      return;
   }

   SendMessage(Sound::kCommand[Sound::REL], snd);
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetAbsolute(Sound* snd)
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert(snd);

   ALuint src = snd->GetSource();
   if(alIsSource(src) == AL_FALSE || !snd->IsInitialized())
   {
      // sound is not playing
      // set flag and bail
      snd->SetState(Sound::ABS);
      snd->ResetState(Sound::REL);
      return;
   }

   alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);   
   if (CheckForError("AudioManager: alSourcei(AL_SOURCE_RELATIVE) error", __FUNCTION__, __LINE__))
   {
      return;
   }

   SendMessage(Sound::kCommand[Sound::ABS], snd);
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetGain( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }


   alSourcef( src, AL_GAIN, static_cast<ALfloat>(snd->GetGain()) );
   if (CheckForError("AudioManager: alSourcef(AL_GAIN) error", __FUNCTION__, __LINE__))
      return;

   SendMessage( Sound::kCommand[Sound::GAIN], snd );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetPitch( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }


   alSourcef( src, AL_PITCH, static_cast<ALfloat>(snd->GetPitch()) );
   if (CheckForError("AudioManager: alSourcef(AL_PITCH) error", __FUNCTION__, __LINE__))
      return;

   SendMessage( Sound::kCommand[Sound::PITCH], snd );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetPosition( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }

   osg::Vec3   pos;
   snd->GetPosition( pos );


   alSource3f( src, AL_POSITION, static_cast<ALfloat>(pos[0]), static_cast<ALfloat>(pos[1]), static_cast<ALfloat>(pos[2]) );
   if (CheckForError("AudioManager: alSource3f(AL_POSITION) error", __FUNCTION__, __LINE__))
      return;

   SendMessage( Sound::kCommand[Sound::POSITION], snd );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetDirection( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }

   osg::Vec3   dir;
   snd->GetDirection( dir );

   alSource3f( src, AL_DIRECTION, static_cast<ALfloat>(dir[0]), static_cast<ALfloat>(dir[1]), static_cast<ALfloat>(dir[2]) );
   if (CheckForError("AudioManager: alSource3f(AL_DIRECTION) error", __FUNCTION__, __LINE__))
      return;

   SendMessage( Sound::kCommand[Sound::DIRECTION], snd );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetVelocity( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, bail
      return;
   }

   osg::Vec3   velo;
   snd->GetVelocity( velo );


   alSource3f( src, AL_VELOCITY, static_cast<ALfloat>(velo[0]), static_cast<ALfloat>(velo[1]), static_cast<ALfloat>(velo[2]) );
   if (CheckForError("AudioManager: alSource3f(AL_VELOCITY) error", __FUNCTION__, __LINE__))
      return;

   SendMessage( Sound::kCommand[Sound::VELOCITY], snd );
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetReferenceDistance( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, set flag and bail
      snd->SetState( Sound::MIN_DIST );
      return;
   }

   ALfloat  min_dist(static_cast<ALfloat>(snd->GetMinDistance()));
   ALfloat  max_dist(static_cast<ALfloat>(snd->GetMaxDistance()));
   min_dist = min_dist; //no-op to prevent warnings
   max_dist = max_dist; //no-op to prevent warnings
   assert( min_dist <= max_dist );

   alSourcef( src, AL_REFERENCE_DISTANCE, min_dist );
   if (CheckForError("AudioManager: alSourcef(AL_REFERENCE_DISTANCE) error", __FUNCTION__, __LINE__))
      return;
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetMaximumDistance( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, set flag and bail
      snd->SetState( Sound::MIN_DIST );
      return;
   }

   ALfloat  min_dist(static_cast<ALfloat>(snd->GetMinDistance()));
   ALfloat  max_dist(static_cast<ALfloat>(snd->GetMaxDistance()));
   min_dist = min_dist; //no-op to prevent warnings
   max_dist = max_dist; //no-op to prevent warnings
   assert( min_dist <= max_dist );


   alSourcef( src, AL_MAX_DISTANCE, max_dist );
   if (CheckForError("AudioManager: alSourcef(AL_MAX_DISTANCE) error", __FUNCTION__, __LINE__))
      return;
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetRolloff( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, set flag and bail
      snd->SetState( Sound::ROL_FACT );
      return;
   }


   alSourcef( src, AL_ROLLOFF_FACTOR, static_cast<ALfloat>(snd->GetRolloffFactor()) );
   if (CheckForError("AudioManager: alSourcef(AL_ROLLOFF_FACTOR) error", __FUNCTION__, __LINE__))
      return;
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetMinimumGain( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, set flag and bail
      snd->SetState( Sound::MIN_GAIN );
      return;
   }

   ALfloat  min_gain(static_cast<ALfloat>(snd->GetMinGain()));

   if ( min_gain <= snd->GetMaxGain() )
   {

      alSourcef( src, AL_MIN_GAIN, min_gain );
      if (CheckForError("AudioManager: alSourcef(AL_MIN_GAIN) error", __FUNCTION__, __LINE__))
         return;
   }
}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::SetMaximumGain( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   assert( snd );

   ALuint src = snd->GetSource();
   if ( alIsSource( src ) == AL_FALSE || !snd->IsInitialized() )
   {
      // sound is not playing, set flag and bail
      snd->SetState( Sound::MAX_GAIN );
      return;
   }

   ALfloat  max_gain(static_cast<ALfloat>(snd->GetMaxGain()));

   if ( snd->GetMinGain() <= max_gain )
   {

      alSourcef( src, AL_MAX_GAIN, max_gain );
      if (CheckForError("AudioManager: alSourcef(AL_MAX_GAIN) error ", __FUNCTION__, __LINE__))
         return;
   }
}

////////////////////////////////////////////////////////////////////////////////
bool AudioManager::SetupSource( Sound* snd )
{
   ALuint   src(0L);

   assert( snd );

   if ( mAvailable.size() )
   {
      src   = mAvailable.front();
      mAvailable.pop();
      assert( alIsSource( src ) == AL_TRUE );

      snd->SetSource( src );
      snd->SetInitialized( true );
      mSourceMap[src] = snd;
      return true;
   }
   else
   {
      return false;
   }

}

////////////////////////////////////////////////////////////////////////////////
void AudioManager::FreeSource( Sound* snd )
{
   CheckForError(ERROR_CLEARING_STRING, __FUNCTION__, __LINE__);
   
   if (snd == NULL || snd->IsInitialized() == false)
   {
      return;
   }

   ALuint src(snd->GetSource());

   // This code was previously: snd->Source( 0L );
   // However, since 0L is a valid source that OpenAL can assign,
   // we cannot enforce that a source of 0 means we "have no source".
   // Instead, we reset it's initialized state.
   snd->SetInitialized( false );

   if ( alIsSource( src ) == AL_FALSE )
      return;

   alSourceStop( src );
   alSourceRewind( src );
   alSourcei( src, AL_LOOPING, AL_FALSE );
   alSourcei( src, AL_BUFFER, AL_NONE );
   mSourceMap[src]   = NULL;
   mAvailable.push( src );
   CheckForError("FreeSource", __FUNCTION__, __LINE__);
}
