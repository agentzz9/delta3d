#ifndef  DELTA_LISTENER
#define  DELTA_LISTENER

#include <transformable.h>



namespace   dtAudio
{
   class DT_EXPORT Listener :  public   dtCore::Transformable
   {
      DECLARE_MANAGEMENT_LAYER(Listener)

      protected:
                        Listener();
         virtual        ~Listener();

      public:
         virtual  void  SetVelocity( const sgVec3& velocity )        = 0L;
         virtual  void  GetVelocity( sgVec3& velocity )        const = 0L;

         virtual  void  SetGain( float gain )                        = 0L;
         virtual  float GetGain( void )                        const = 0L;
   };
};



#endif   // DELTA_LISTENER