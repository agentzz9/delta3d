// effectmanager.cpp: Implementation of the EffectManager class.
//
//////////////////////////////////////////////////////////////////////
#include <prefix/dtcoreprefix-src.h>
#include <dtCore/effectmanager.h>
#include <dtCore/transformable.h>
#include <dtCore/system.h>
#include <dtCore/globals.h>
#include <dtUtil/log.h>
#include <dtUtil/matrixutil.h>
#include <dtUtil/stringutils.h>

#include <osg/Matrix>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Group>
#include <osg/Node>
#include <osg/NodeVisitor>

#include <osgDB/ReadFile>
#include <osgParticle/Emitter>

#include <cassert>

namespace dtCore
{
   IMPLEMENT_MANAGEMENT_LAYER(EffectManager)

   /**
    * A visitor class that finds matrix transforms and sets their
    * positions to the given value.
    */
   class PositionVisitor : public osg::NodeVisitor
   {
      public:

         PositionVisitor(osg::Vec3 position) : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
            mPosition(position)
         {
         }

         virtual void apply(osg::MatrixTransform& node)
         {
            osg::Matrix matrix;
            matrix.makeTranslate(mPosition);
            node.setMatrix(matrix);
         }

      private:

         osg::Vec3 mPosition;
   };

   /**
    * A callback class that updates the state of a detonation.
    */
   class DetonationUpdateCallback : public osg::NodeCallback
   {
      public:
      
         DetonationUpdateCallback(Detonation* detonation) : 
            mDetonation(detonation)
         {
         }
         
         virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
         {
            assert( mDetonation.valid() );

            osg::Vec3 position;
            
            mDetonation->GetPosition(position);
            
            if(mDetonation->GetParent() != 0)
            {
               Transform transform;
               
               mDetonation->GetParent()->GetTransform(transform);
               
               osg::Matrix mat;
               transform.Get(mat);
               dtUtil::MatrixUtil::TransformVec3(position, mat);
            }

            PositionVisitor pv = PositionVisitor( osg::Vec3(position[0], position[1], position[2] ) );
            node->accept( pv );
            
            traverse(node, nv);
         }
         
      private:
      
         dtCore::RefPtr<Detonation> mDetonation;
   };


   /**
    * Constructor.
    *
    * @param name the instance name
    */
   EffectManager::EffectManager(const std::string& name) : DeltaDrawable(name),
      mLastTime(0.0)
   {
      RegisterInstance(this);     
      mGroup = new osg::Group;
      AddSender(&System::GetInstance());
   }

   /**
    * Destructor.
    */
   EffectManager::~EffectManager()
   {
      DeregisterInstance(this);
      RemoveSender(&System::GetInstance());
   }

   /**
    * Maps the specified detonation name to the given filename. It will
    * replace any existing filename bound to the detonationName string.
    *
    * @param detonationName the detonation name to map
    * @param filename the filename corresponding to the detonation type
    */
   void EffectManager::AddDetonationTypeMapping(const std::string& detonationName, 
                                                const std::string& filename)
   {
      // Use operator[] since we want to insert/replace 
      mDetonationTypeFilenameMap[detonationName] = filename;
   }



   /**
    * Removes the mapping for the given detonation name.
    *
    * @param detonationName the detonation name to unmap
    */
   void EffectManager::RemoveDetonationTypeMapping(const std::string& detonationName)
   {
      mDetonationTypeFilenameMap.erase( detonationName );
   }


   /**
    * Returns the number of active effects.
    *
    * @return the number of active effects
    */
   int EffectManager::GetEffectCount() const
   {
      return mEffects.size();
   }

   /**
    * Returns the effect at the specified index.
    *
    * @param index the index of the effect to retrieve
    * @return the effect at the specified index
    */
   const Effect* EffectManager::GetEffect(int index) const
   {
      return mEffects[index].get();
   }

   /**
    * Adds a new detonation effect.
    *
    * @param position the position of the detonation
    * @param type the name of the detonation
    * @param timeToLive the lifespan of the detonation, in seconds,
    * or 0.0 for unlimited
    * @param parent the parent of the detonation, or 0 for
    * none
    * @return a pointer to the detonation object
    */
   Detonation* EffectManager::AddDetonation( const osg::Vec3& position,
                                             const std::string& detonationName,
                                             double timeToLive,
                                             Transformable* parent)
   {
      StringMap::iterator found = mDetonationTypeFilenameMap.find(detonationName);
      if(found != mDetonationTypeFilenameMap.end())
      {
         RefPtr<osgDB::ReaderWriter::Options> options = new osgDB::ReaderWriter::Options;
         options->setObjectCacheHint( osgDB::ReaderWriter::Options::CACHE_IMAGES );
         osg::ref_ptr<osg::Node> node;

         std::string psFile = dtCore::FindFileInPathList(found->second);
         if (psFile.empty())
         {
            LOG_WARNING("Can't find particle effect file:" + found->second);
            return 0;
         }
         else
         {
            node = osgDB::readNodeFile( psFile, options.get() );
         }

         if (!node.valid() ) 
         {
            LOG_WARNING("Can't load particle effect:" + found->second);
            return 0;
         }
        
         Detonation* detonation = new Detonation(node.get(), timeToLive, position, detonationName, parent);

         if(parent != 0)
         {
            node->setUpdateCallback( new DetonationUpdateCallback(detonation) );
         }
         else
         {
            PositionVisitor pv = PositionVisitor( osg::Vec3( position[0], position[1], position[2] ) );
            node->accept( pv );
         }
         
         AddEffect(detonation);

         return detonation;
      }

      return 0;
   }


   /**
    * Adds an effect to this manager.
    *
    * @param effect the effect to add
    */
   void EffectManager::AddEffect(Effect* effect)
   {
      assert(effect);
      mEffects.push_back(effect);

      if(effect->GetNode() != 0)
      {
         mGroup->addChild(effect->GetNode());
      }

      // Replace with something cooler, can be refactored to use the same
      // code as EffectRemoved below
      for(  EffectListenerVector::iterator it = mEffectListeners.begin();
            it != mEffectListeners.end();
            it++)
      {
         (*it)->EffectAdded(this, effect);
      }
   }

   /**
    * Removes an effect from this manager.
    *
    * @param effect the effect to remove
    */
   void EffectManager::RemoveEffect(Effect* effect)
   {      
      // Replace with a for_each algorithm
      for(  EffectVector::iterator it = mEffects.begin();
            it != mEffects.end();
            it++)
      {
         if( it->get() == effect )
         {
            mGroup->removeChild(effect->GetNode());

            mEffects.erase(it);

            // Replace with something cooler, can be refactored to use the same
            // code as EffectAdded above
            for(  EffectListenerVector::iterator it2 = mEffectListeners.begin();
                  it2 != mEffectListeners.end();
                  it2++ )
            {
               (*it2)->EffectRemoved(this, effect);
            }

            return;
         }
      }
   }
   
   template< typename T >
   struct IsPointer : public std::binary_function< dtCore::RefPtr<T>, T*, bool >
   {
      bool operator()( const dtCore::RefPtr<T>& refPtr, const T* ptr ) const
      {
         return refPtr.get() == ptr;
      }
   };

   /**
    * Adds a listener for effect events.
    *
    * @param effectListener the listener to add
    */
   void EffectManager::AddEffectListener( EffectListener* effectListener )
   {
      EffectListenerVector::iterator found = std::find_if(  mEffectListeners.begin(), 
                                                            mEffectListeners.end(), 
                                                            std::bind2nd( IsPointer<EffectListener>(),
                                                                          effectListener ) );
      if( found == mEffectListeners.end() )
      {
         mEffectListeners.push_back( effectListener );
      }
   }

   /**
    * Removes a listener for effect events.
    *
    * @param effectListener the listener to remove
    */
   void EffectManager::RemoveEffectListener( EffectListener* effectListener )
   {
      mEffectListeners.erase( std::remove_if( mEffectListeners.begin(),
                                              mEffectListeners.end(),
                                              std::bind2nd( IsPointer<EffectListener>(),
                                                            effectListener ) ),
                              mEffectListeners.end() );
   }

   /**
    * Returns this object's OpenSceneGraph node.
    *
    * @return the OpenSceneGraph node
    */
   osg::Node* EffectManager::GetOSGNode()
   {
      return mGroup.get();
   }

   const osg::Node* EffectManager::GetOSGNode() const
   {
      return mGroup.get();
   }

   /**
    * Finds and returns the maximimum lifetime of the particles
    * whose systems lie under the specified node.
    *
    * @param effectNode the effect node to search
    * @return the maximum particle lifetime, or 0.0 if no
    * particle systems lie under the node
    */
   static double FindMaximumParticleLifeTime(osg::Node* effectNode)
   {
      double maximumLifeTime = 0.0;

      // Factor out these dynamic_casts with a visitor
      if(osg::Group* group = dynamic_cast<osg::Group*>(effectNode))
      {
         for(unsigned int i=0;i<group->getNumChildren();i++)
         {
            double lifeTime = FindMaximumParticleLifeTime(group->getChild(i));

            if(lifeTime > maximumLifeTime)
            {
               maximumLifeTime = lifeTime;
            }
         }
      }
      else if(osg::Geode* geode = dynamic_cast<osg::Geode*>(effectNode))
      {
         for(unsigned int i=0;i<geode->getNumDrawables();i++)
         {
            osg::Drawable* drawable = geode->getDrawable(i);

            if(osgParticle::ParticleSystem* particleSystem = dynamic_cast<osgParticle::ParticleSystem*>(drawable))
            {
               maximumLifeTime = particleSystem->getDefaultParticleTemplate().getLifeTime();
            }
         }
      }

      return maximumLifeTime;
   }

   /**
    * Deletes all particle emitters under the specified node.
    *
    * @param effectNode the effect node to modify
    */
   static void DeleteParticleEmitters(osg::Node* effectNode)
   {
      if(osg::Group* group = dynamic_cast<osg::Group*>(effectNode))
      {
         typedef std::vector< osg::Node* > NodeVector;
         NodeVector nodesToRemove;

         for( unsigned int i = 0; i < group->getNumChildren(); i++ )
         {
            osg::Node* node = group->getChild(i);
            
            // Factor out these dynamic_casts with a visitor
            if( dynamic_cast<osgParticle::Emitter*>(node) )  
            {
               nodesToRemove.push_back(node);
            }
            else if( dynamic_cast<osg::Group*>(node) ) 
            {
               DeleteParticleEmitters(node);
            }
         }

         // Refactor into an alogrithm
         for(  NodeVector::iterator it = nodesToRemove.begin();
               it != nodesToRemove.end();
               it++)
         {
            group->removeChild(*it);
         }
      }
   }

   /**
    * Processes a received message.
    *
    * @param data the message structure
    */
   void EffectManager::OnMessage(MessageData *data)
   {
      if(data->message == dtCore::System::MESSAGE_PRE_FRAME)
      {
         const double delta = *static_cast<const double*>(data->userData);

         if(mLastTime != 0)
         {
            EffectVector effectsToRemove;
            effectsToRemove.reserve( mEffects.size() );

            // Refactor this into a unary_functor
            for(  EffectVector::iterator it = mEffects.begin();
                  it != mEffects.end();
                  it++)
            {
               assert( it->valid() );
               double ttl = (*it)->GetTimeToLive();
               
               if(ttl != 0.0)
               {
                  ttl -= delta;

                  if(ttl <= 0.0)
                  {
                     if((*it)->IsDying())
                     {
                        effectsToRemove.push_back(*it);
                     }
                     else
                     {
                        double maxLifeTime = FindMaximumParticleLifeTime( (*it)->GetNode() );

                        if(maxLifeTime == 0.0)
                        {
                           effectsToRemove.push_back(*it);
                        }
                        else
                        {
                           DeleteParticleEmitters( (*it)->GetNode() );
                           (*it)->SetDying(true);
                           (*it)->SetTimeToLive(maxLifeTime);
                        }
                     }
                  }
                  else
                  {
                     (*it)->SetTimeToLive(ttl);
                  }
               }
            }

            // Replace with a std::for_each
            for(  EffectVector::iterator it2 = effectsToRemove.begin();
                  it2 != effectsToRemove.end();
                  it2++)
            {
               RemoveEffect(it2->get());
            }
         }

         mLastTime += delta;
      }
   }

   DetonationType StringToDetonationType( const std::string& stringType )
   {
      if( stringType == "HighExplosiveDetonation" )
      {
         return HighExplosiveDetonation;
      }
      else if( stringType == "SmokeDetonation" )
      {
         return SmokeDetonation;
      }
      else if( stringType == "WP" )
      {
         return WP;
      }      
      else if( stringType == "VT" )
      {
         return VT;
      }
      else if( stringType == "ICM" )
      {
         return ICM;
      }
      else if( stringType == "M825" )
      {
         return M825;
      }
      else
      {
         return HighExplosiveDetonation;
      }
   }
   
   std::string DetonationTypeToString( DetonationType detonationType )
   {
      switch( detonationType )
      {
         case HighExplosiveDetonation :
         {
            return "HighExplosiveDetonation";
            break;
         }
         case SmokeDetonation :
         {
            return "SmokeDetonation";
            break;
         }
         case WP :
         {
            return "WP";
            break;
         }
         case VT:
         {
            return "VT";
            break;
         }         
         case ICM:
         {
            return "ICM";
            break;
         }         
         case M825:
         {
            return "M825";
            break;
         }         
         default:
         {
            return "HighExplosiveDetonation";
            break;
         }
      }
   }

   /**
    * Constructor.
    *
    * @param node the effect's OpenSceneGraph node
    * @param timeToLive the lifespan of the effect in
    * seconds, or 0.0 for unlimited
    */
   Effect::Effect(osg::Node* node, double timeToLive) : mNode(node),
      mTimeToLive(timeToLive),
      mDying(false)
   {
   }

   /**
    * Destructor.
    */
   Effect::~Effect()
   {
   }

   /**
    * Returns the effect's OpenSceneGraph node.
    *
    * @return the effect's OpenSceneGraph node
    */
   osg::Node* Effect::GetNode()
   {
      return mNode.get();
   }

   const osg::Node* Effect::GetNode() const
   {
      return mNode.get();
   }

   /** 
    * Sets the remaining lifespan of this effect.
    *
    * @param timeToLive the remaining lifespan, in seconds,
    * or 0.0 for unlimited
    */
   void Effect::SetTimeToLive(double timeToLive)
   {
      mTimeToLive = timeToLive;
   }

   /**
    * Returns the remaining lifespan of this effect.
    *
    * @return the remaining lifespan, in seconds, or 0.0
    * for unlimited
    */
   double Effect::GetTimeToLive()
   {
      return mTimeToLive;
   }

   /**
    * Sets the dying flag.
    *
    * @param dying the new value of the dying flag
    */
   void Effect::SetDying(bool dying)
   {
      mDying = dying;
   }

   /**
    * Checks the dying flag.
    *
    * @return the value of the dying flag
    */
   bool Effect::IsDying()
   {
      return mDying;
   }

   /**
    * Constructor.
    *
    * @param node the particle system node
    * @param timeToLive the lifespan of the detonation, or
    * 0.0 for unlimited
    * @param position the position of the detonation
    * @param detonationName the name of the detonation
    * @param parent the parent of the detonation, or 0
    * for none
    */
   Detonation::Detonation( osg::Node* node,
                           double timeToLive,
                           const osg::Vec3& position,
                           const std::string& detonationName,
                           Transformable* parent) : Effect(node, timeToLive),
      mPosition( position ),
      mDetonationName(detonationName),
      mParent(parent)
   {
   }

   /**
    * Retrieves the position of this detonation.
    *
    * @param result a vector to hold the result
    */
   void Detonation::GetPosition(osg::Vec3& result) const
   {
      result.set( mPosition );
   }

   const osg::Vec3& Detonation::GetPosition() const
   {
      return mPosition;
   }

   /**
    * Returns the type of this detonation.
    *
    * @return the type of this detonation
    */
   const std::string& Detonation::GetType()
   {
      return mDetonationName;
   }

   void Detonation::GetType( DetonationType& detonationType )
   {
      detonationType = StringToDetonationType( mDetonationName );
   }

   /**
    * Returns the Transformable parent of the detonation, or
    * 0 if the detonation is unparented.
    *
    * @return the parent of the detonation
    */
   Transformable* Detonation::GetParent()
   {
      return mParent.get();
   }

   const Transformable* Detonation::GetParent() const
   {
      return mParent.get();
   }
}
