/* -*-c++-*-
* testbumpmap - testbumpmap (.h & .cpp) - Using 'The MIT License'
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include "testbumpmap.h"

#include <dtCore/globals.h>
#include <dtCore/camera.h>
#include <dtCore/scene.h>
#include <dtCore/shadermanager.h>

#include <osg/Drawable>
#include <osg/Program>
#include <osg/Texture2D>
#include <osg/Geode>
#include <osgGA/GUIEventAdapter>
#include <osgDB/ReadFile>
#include <osgUtil/TangentSpaceGenerator>

////////////////////////////////////////////////////////////////////////////////

// Node visitor that gather all geometry contained within a subgraph
class GeometryCollector : public osg::NodeVisitor
{
public:

   GeometryCollector()
      : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN){}

   virtual void apply(osg::Geode& node)
   {
      // start off with an empty list
      mGeomList.clear();

      int numberOfDrawables = node.getNumDrawables();

      for (int drawableIndex = 0; drawableIndex < numberOfDrawables; ++drawableIndex)
      {
         // If this is geometry, get a pointer to it
         osg::Geometry* geom = node.getDrawable(drawableIndex)->asGeometry();

         if (geom)
         {
            // store the geometry in this list
            mGeomList.push_back(geom);
         }
      }

      traverse(node);
   }

   std::vector<osg::Geometry*> mGeomList;
};

////////////////////////////////////////////////////////////////////////////////
TestBumpMapApp::TestBumpMapApp(const std::string& customObjectName, const std::string& configFilename /*= "config.xml"*/)
   : Application(configFilename)
   , mTotalTime(0.0f)
   , mDiffuseTexture(NULL)
   , mNormalTexture(NULL)
{
   //load the xml file which specifies our shaders
   dtCore::ShaderManager& sm = dtCore::ShaderManager::GetInstance();
   sm.LoadShaderDefinitions("shaders/ShaderDefinitions.xml");

   // Load our art assets
   LoadTextures();
   LoadGeometry(customObjectName);  

   dtCore::ShaderParamInt* customMode = NULL;
   dtCore::ShaderParamInt* sphereMode = NULL;

   // Assign the bump shader to the nodes
   AssignShaderToObject(mSphere.get(), sphereMode);
   AssignShaderToObject(mCustomObject.get(), customMode);  

   // Store pointers to the mode for toggling different paths in the shader
   mCustomShaderMode = customMode;
   mSphereShaderMode = sphereMode;

   dtCore::Transform objectTransform;
   mCustomObject->GetTransform(objectTransform);     

   // Apply the motion model to control the light centered around our object
   mOrbitMotion = new dtCore::OrbitMotionModel(GetKeyboard(), GetMouse());
   mOrbitMotion->SetTarget(GetScene()->GetLight(0));    

   // Adjust the positioning of the camera depending on the size of the object
   CenterCameraOnObject(mCustomObject.get());
}


////////////////////////////////////////////////////////////////////////////////
void TestBumpMapApp::LoadGeometry(const std::string& customObjectName)
{   
   // Load a sphere a second object to see the effect on
   mSphere = new dtCore::Object("Sphere");
   mSphere->LoadFile("models/physics_happy_sphere.ive");
   mSphere->SetActive(false);
   AddDrawable(mSphere.get());

   mCustomObject = new dtCore::Object("Custom");
   mCustomObject->LoadFile(customObjectName);  
   AddDrawable(mCustomObject.get());

   // Load some geometry to represent the direction of the light
   mLightObject = new dtCore::Object("Happy Sphere");
   mLightObject->LoadFile("models/LightArrow.ive");
   mLightObject->SetScale(osg::Vec3(0.5f, 0.5f, 0.5f));
   AddDrawable(mLightObject.get());  

   // Calculate tangent vectors from the geometry for use in tangent space calculations
   GenerateTangentsForObject(mSphere.get());
   GenerateTangentsForObject(mCustomObject.get());
}

////////////////////////////////////////////////////////////////////////////////
void TestBumpMapApp::LoadTextures()
{   
   osg::Image* diffuseImage = osgDB::readImageFile("textures/sheetmetal.tga");
   osg::Image* normalImage  = osgDB::readImageFile("textures/delta3d_logo_normal_map.tga");

   mDiffuseTexture = new osg::Texture2D;
   mDiffuseTexture->setImage(diffuseImage);
   mDiffuseTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
   mDiffuseTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
   mDiffuseTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
   mDiffuseTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
   mDiffuseTexture->setMaxAnisotropy(8);

   mNormalTexture = new osg::Texture2D;
   mNormalTexture->setImage(normalImage);
   mNormalTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
   mNormalTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
   mNormalTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
   mNormalTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
   mNormalTexture->setMaxAnisotropy(8);
}

////////////////////////////////////////////////////////////////////////////////
bool TestBumpMapApp::KeyPressed(const dtCore::Keyboard* keyboard, int key)
{
   bool verdict(false);

   switch(key)
   {
   case osgGA::GUIEventAdapter::KEY_Escape:
      {
         this->Quit();
         verdict = true;
         break;
      }     
   case osgGA::GUIEventAdapter::KEY_Space:
      {
         static bool renderToggle = false;
         renderToggle = !renderToggle;

         dtCore::Object* current = (renderToggle) ? mSphere.get(): mCustomObject.get();
         CenterCameraOnObject(current);

         mSphere->SetActive(renderToggle);
         mCustomObject->SetActive(!renderToggle);

         break;
      }
   case '1':
   case '2':
   case '3':
   case '4':
   case '5':  
   case '6':
      {
         int value = key - 48;

         // Offset the ascii value to get the numberic one
         mCustomShaderMode->SetValue(value);
         mSphereShaderMode->SetValue(value);

         break;
      }
   }

   return verdict;
}

////////////////////////////////////////////////////////////////////////////////
void TestBumpMapApp::PreFrame(const double deltaFrameTime)
{
   mTotalTime += deltaFrameTime * 0.15f;

   osg::Matrix rotateMat;
   rotateMat.makeRotate(osg::DegreesToRadians(30.0f) * mTotalTime, osg::Vec3(1.0f, 0.0f, 1.0f));

   dtCore::Transform objectTransform;
   mCustomObject->GetTransform(objectTransform);
   objectTransform.SetRotation(rotateMat);

   // lazily set both
   mCustomObject->SetTransform(objectTransform);
   mSphere->SetTransform(objectTransform);   

   // Update the transform of the light arrow to match the light position
   dtCore::Transform lightTransform;
   GetScene()->GetLight(0)->GetTransform(lightTransform);
   mLightObject->SetTransform(lightTransform);
}

////////////////////////////////////////////////////////////////////////////////
void TestBumpMapApp::GenerateTangentsForObject(dtCore::Object* object)
{
   // Override texture values in the geometry to ensure that we can apply normal mapping
   osg::StateSet* ss = object->GetOSGNode()->getOrCreateStateSet();
   ss->setTextureAttributeAndModes(0, mDiffuseTexture.get(), osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
   ss->setTextureAttributeAndModes(1, mNormalTexture.get(), osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);

   // Get all geometry in the graph to apply the shader to
   osg::ref_ptr<GeometryCollector> geomCollector = new GeometryCollector;
   object->GetOSGNode()->accept(*geomCollector);        

   // Calculate tangent vectors for all faces and store them as vertex attributes
   for (size_t geomIndex = 0; geomIndex < geomCollector->mGeomList.size(); ++geomIndex)
   {
      osg::Geometry* geom = geomCollector->mGeomList[geomIndex];

      osg::ref_ptr<osgUtil::TangentSpaceGenerator> tsg = new osgUtil::TangentSpaceGenerator;
      tsg->generate(geom, 0);

      if (!geom->getVertexAttribArray(6))
      {
         geom->setVertexAttribData(6, osg::Geometry::ArrayData(tsg->getTangentArray(), osg::Geometry::BIND_PER_VERTEX, GL_FALSE));
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
void TestBumpMapApp::AssignShaderToObject(dtCore::Object* object, dtCore::ShaderParamInt*& outMode)
{
   dtCore::ShaderManager& sm = dtCore::ShaderManager::GetInstance();
   dtCore::ShaderProgram* sp = sm.FindShaderPrototype("TestBumpMap", "TestShaders");

   if (sp != NULL)
   {
      dtCore::ShaderProgram* boundProgram = sm.AssignShaderFromPrototype(*sp, *object->GetOSGNode());

      // Associate the vertex attribute in location 6 with the name "TangentAttrib"
      osg::Program* osgProgram = boundProgram->GetShaderProgram();
      osgProgram->addBindAttribLocation("TangentAttrib", 6);

      outMode = dynamic_cast<dtCore::ShaderParamInt*>(boundProgram->FindParameter("mode"));
   }
}

////////////////////////////////////////////////////////////////////////////////
void TestBumpMapApp::CenterCameraOnObject(dtCore::Object* object)
{
   osg::Vec3 center;
   float radius;

   object->GetBoundingSphere(&center, &radius);

   // position the camera slightly behind the origin
   dtCore::Transform cameraTransform;
   cameraTransform.SetTranslation(center -osg::Y_AXIS * radius * 4.0f);

   // Move our light icon to the outer bounds of the object
   mOrbitMotion->SetDistance(radius);
   mOrbitMotion->SetFocalPoint(center);  

   GetCamera()->SetTransform(cameraTransform);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
   std::string customObjectName = "models/physics_crate.ive";

   // Allow specifying of custom geometry from the command line
   if (argc > 1)
   {
      customObjectName = std::string(argv[1]);
   }

   std::string dataPath = dtCore::GetDeltaDataPathList();
   dtCore::SetDataFilePathList(dataPath + ";" +
                               dtCore::GetDeltaRootPath() + "/examples/data" + ";" +
                               dtCore::GetDeltaRootPath() + "/examples/testBumpMap");

   dtCore::RefPtr<TestBumpMapApp> app = new TestBumpMapApp(customObjectName);
   app->Config();
   app->Run();

   return 0;
}
