#ifndef  DELTA_AUDIOMANAGER
#define  DELTA_AUDIOMANAGER

#include <stddef.h>
#include <vector>
#include <queue>
#include <map>
#include <string>

#include <al.h>
#include <sg.h>

#include <base.h>
#include <transformable.h>
#include <listener.h>



// forward references
struct   AudioConfigData;



namespace   dtAudio
{
   class Sound;

   // dtAudio::Sound should really be broken out
   // into it's own file, replacing dtCore::Sound

   //! callback function type
   typedef  void  (*SoundCB)( Sound* sound, void *param );

   class DT_EXPORT Sound :  public   dtCore::Transformable
   {
      DECLARE_MANAGEMENT_LAYER(Sound)

      public:
                  enum  Command
                        {
                           NONE     = 0L,
                           LOAD,
                           UNLOAD,
                           PLAY,
                           PAUSE,
                           STOP,
                           REWIND,
                           LOOP,
                           UNLOOP,
                           QUEUE,
                           GAIN,
                           PITCH,
                           POSITION,
                           DIRECTION,
                           VELOCITY,
                           ABS,
                           REL,

                           kNumCommands
                        };

      public:
         static   const char* kCommand[kNumCommands];

      protected:
                              Sound();
         virtual              ~Sound();

         virtual  void        OnMessage( MessageData* data );

      public:
         virtual  const char* GetFilename( void )              {  return   mFilename.c_str();   }

         virtual  void        SetPlayCallback( SoundCB cb, void* param );
         virtual  void        SetStopCallback( SoundCB cb, void* param );

         virtual  void        LoadFile( const char* file );
         virtual  void        UnloadFile( void );

         virtual  void        Play( void );
         virtual  void        Pause( void );
         virtual  void        Stop( void );
         virtual  void        Rewind( void );

         virtual  bool        IsPlaying( void )          const {  return   false;   }
         virtual  bool        IsPaused( void )           const {  return   false;   }
         virtual  bool        IsStopped( void )          const {  return   true;    }

         virtual  void        SetLooping( bool loop = true );
         virtual  bool        IsLooping( void )          const {  return   false;   }

         virtual  void        ListenerRelative( bool relative );
         virtual  bool        IsListenerRelative( void ) const {  return   false;   }

         virtual  void        SetGain( float gain );
         virtual  float       GetGain( void )            const {  return   static_cast<float>(mGain);    }

         virtual  void        SetPitch( float pitch );
         virtual  float       GetPitch( void )           const {  return   static_cast<float>(mPitch);   }

         virtual  void        SetTransform(  dtCore::Transform*                  xform,
                                             dtCore::Transformable::CoordSysEnum cs = dtCore::Transformable::ABS_CS );

         virtual  void        SetPosition( const sgVec3& position );
         virtual  void        GetPosition( sgVec3& position )     const;

         virtual  void        SetDirection( const sgVec3& direction );
         virtual  void        GetDirection( sgVec3& direction )   const;

         virtual  void        SetVelocity( const sgVec3& velocity );
         virtual  void        GetVelocity( sgVec3& velocity )     const;

      protected:
                  std::string    mFilename;
                  SoundCB        mPlayCB;
                  void*          mPlayCBData;
                  SoundCB        mStopCB;
                  void*          mStopCBData;
                  float          mGain;
                  float          mPitch;
                  sgVec3         mPos;
                  sgVec3         mDir;
                  sgVec3         mVelo;
   };



   class DT_EXPORT AudioManager   :  public   dtCore::Base
   {
        DECLARE_MANAGEMENT_LAYER(AudioManager)

      private:
         struct   BufferData
         {
            ALuint         buf;
            const char*    file;
            ALboolean      loop;
            unsigned int   use;

            BufferData()
            :  buf(0L),
               file(""),
               loop(AL_FALSE),
               use(0L)
            {}
         };

         class SoundObj :  public   Sound
         {
            DECLARE_MANAGEMENT_LAYER(SoundObj)

            private:
               typedef  std::queue<const char*>   CMD_QUE;

            public:
                                       SoundObj();
               virtual                 ~SoundObj();

               virtual  void           OnMessage( MessageData* data );

               virtual  void           SetParent( dtCore::Transformable* parent );

               virtual  bool           IsPlaying( void )          const;
               virtual  bool           IsPaused( void )           const;
               virtual  bool           IsStopped( void )          const;
               virtual  bool           IsLooping( void )          const;
               virtual  bool           IsListenerRelative( void ) const;

                        void           Command( const char* cmd );
                        const char*    Command( void );

                        void           Buffer( ALuint buffer );
                        ALuint         Buffer( void );

                        void           Source( ALuint source );
                        ALuint         Source( void );

                        void           SetState( unsigned int flag );
                        void           ResetState( unsigned int flag );

                        void           Clear( void );

            private:
                        CMD_QUE        mCommand;          
                        ALuint         mBuffer;
                        ALuint         mSource;
                        unsigned int   mState;
         };

         class ListenerObj :  public   Listener
         {
            DECLARE_MANAGEMENT_LAYER(ListenerObj)

            public:
                                 ListenerObj();
               virtual           ~ListenerObj();

               virtual  void     SetVelocity( const sgVec3& velocity );
               virtual  void     GetVelocity( sgVec3& velocity )  const;

               virtual  void     SetGain( float gain );
               virtual  float    GetGain( void )   const;

               virtual  void     OnMessage( MessageData* data );
               virtual  void     SetParent( dtCore::Transformable* parent );
               virtual  void     Clear( void );

            private:
                        ALfloat  mVelo[3L];
                        ALfloat  mGain;
         };

      private:
         typedef  osg::ref_ptr<AudioManager>          MOB_ptr;
         typedef  osg::ref_ptr<SoundObj>              SOB_PTR;
         typedef  osg::ref_ptr<ListenerObj>           LOB_PTR;

         typedef  std::map<std::string, BufferData*>  BUF_MAP;

         typedef  std::map<ALuint, SOB_PTR>           SRC_MAP;
         typedef  std::queue<ALuint>                  SRC_QUE;
         typedef  std::vector<ALuint>                 SRC_LST;

         typedef  std::queue<SOB_PTR>                 SND_QUE;
         typedef  std::vector<SOB_PTR>                SND_LST;

      private:
         static   MOB_ptr                 _Mgr;
         static   LOB_PTR                 _Mic;
         static   const char*             _EaxVer;
         static   const char*             _EaxSet;
         static   const char*             _EaxGet;
         static   const AudioConfigData   _DefCfg;

      private:
                                    AudioManager( std::string name = "audiomanager" );
         virtual                    ~AudioManager();

      public:
         static   void              Instantiate( void );
         static   void              Destroy( void );
         static   AudioManager*     GetManager( void );
         static   Listener*         GetListener( void );

         virtual  void              Config( const AudioConfigData& data = _DefCfg );
         virtual  void              OnMessage( MessageData* data );

                  Sound*            NewSound( void );
                  void              FreeSound( Sound*& sound );

                  bool              LoadWaveFile( const char* file );
                  bool              UnloadWaveFile( const char* file );

      private:
         inline   void              PreFrame( const double deltaFrameTime );
         inline   void              Frame( const double deltaFrameTime );
         inline   void              PostFrame( const double deltaFrameTime );
         inline   bool              Configured( void )   const;
         inline   bool              ConfigSources( unsigned int num );
         inline   bool              ConfigEAX( bool eax );
         inline   void              LoadSound( SoundObj* snd );
         inline   void              UnloadSound( SoundObj* snd );
         inline   void              PlaySound( SoundObj* snd );
         inline   void              PauseSound( SoundObj* snd );
         inline   void              StopSound( SoundObj* snd );
         inline   void              RewindSound( SoundObj* snd );
         inline   void              SetLoop( SoundObj* snd );
         inline   void              ResetLoop( SoundObj* snd );
         inline   void              SetRelative( SoundObj* snd );
         inline   void              SetAbsolute( SoundObj* snd );
         inline   void              SetGain( SoundObj* snd );
         inline   void              SetPitch( SoundObj* snd );
         inline   void              SetPosition( SoundObj* snd );
         inline   void              SetDirection( SoundObj* snd );
         inline   void              SetVelocity( SoundObj* snd );
         inline   bool              GetSource( SoundObj* snd );
         inline   void              FreeSource( SoundObj* snd );

      private:
                  ALvoid*           mEAXSet;
                  ALvoid*           mEAXGet;

                  ALsizei           mNumSources;
                  ALuint*           mSource;

                  SRC_MAP           mSourceMap;
                  SRC_QUE           mAvailable;
                  SRC_QUE           mPlayQueue;
                  SRC_QUE           mPauseQueue;
                  SRC_QUE           mStopQueue;
                  SRC_QUE           mRewindQueue;
                  SRC_LST           mActiveList;

                  BUF_MAP           mBufferMap;

                  SND_QUE           mSoundCommand;
                  SND_QUE           mSoundRecycle;
                  SND_LST           mSoundList;
   };
};



// configuration data
struct DT_EXPORT AudioConfigData
{
   enum           DistanceModel
                  {
                     dmNONE      = AL_NONE,
                     dmINVERSE   = AL_INVERSE_DISTANCE,
                     dmINVCLAMP  = AL_INVERSE_DISTANCE_CLAMPED,
                  };

   unsigned int   numSources;
   bool           eax;
   unsigned int   distancemodel;

   AudioConfigData(  unsigned int   ns = 16L,
                     bool           ex = false,
                     unsigned int   dm = dmINVERSE )
   :  numSources(ns),
      eax(ex),
      distancemodel(dm)
   {}
};



#endif   // DELTA_AUDIOMANAGER
