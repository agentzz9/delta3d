/*
 * Delta3D Open Source Game and Simulation Engine
 * Copyright (C) 2008 MOVES Institute
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Jeff P. Houde
 */
#include <prefix/dtdirectornodesprefix.h>
#include <dtDirectorNodes/comparevectoraction.h>

#include <dtDAL/floatactorproperty.h>
#include <dtDAL/vectoractorproperties.h>

#include <dtDirector/director.h>

#include <dtUtil/mathdefines.h>

namespace dtDirector
{
   ////////////////////////////////////////////////////////////////////////////////
   CompareVectorAction::CompareVectorAction()
      : ActionNode()
      , mEpsilon(FLT_EPSILON)
   {
      AddAuthor("Eric R. Heine");
   }

   ////////////////////////////////////////////////////////////////////////////////
   CompareVectorAction::~CompareVectorAction()
   {
   }

   ////////////////////////////////////////////////////////////////////////////////
   void CompareVectorAction::Init(const NodeType& nodeType, DirectorGraph* graph)
   {
      ActionNode::Init(nodeType, graph);

      mOutputs.clear();
      mOutputs.push_back(OutputLink(this, "A == B"));
      mOutputs.push_back(OutputLink(this, "A != B"));
      mOutputs.push_back(OutputLink(this, "A equivalent to B"));
      mOutputs.push_back(OutputLink(this, "A not equivalent to B"));
   }

   ////////////////////////////////////////////////////////////////////////////////
   void CompareVectorAction::BuildPropertyMap()
   {
      ActionNode::BuildPropertyMap();

      // Create our value links.
      dtDAL::Vec4ActorProperty* leftProp = new dtDAL::Vec4ActorProperty(
         "A", "A",
         dtDAL::Vec4ActorProperty::SetFuncType(this, &CompareVectorAction::SetA),
         dtDAL::Vec4ActorProperty::GetFuncType(this, &CompareVectorAction::GetA),
         "Value A.");

      dtDAL::Vec4ActorProperty* rightProp = new dtDAL::Vec4ActorProperty(
         "B", "B",
         dtDAL::Vec4ActorProperty::SetFuncType(this, &CompareVectorAction::SetB),
         dtDAL::Vec4ActorProperty::GetFuncType(this, &CompareVectorAction::GetB),
         "Value B.");

      dtDAL::FloatActorProperty* epsilonProp = new dtDAL::FloatActorProperty(
         "Epsilon", "Epsilon",
         dtDAL::FloatActorProperty::SetFuncType(this, &CompareVectorAction::SetEpsilon),
         dtDAL::FloatActorProperty::GetFuncType(this, &CompareVectorAction::GetEpsilon),
         "Epsilon for equivalency check.");

      AddProperty(leftProp);
      AddProperty(rightProp);
      AddProperty(epsilonProp);

      // This will expose the properties in the editor and allow
      // them to be connected to ValueNodes.
      mValues.push_back(ValueLink(this, leftProp, false, false, false));
      mValues.push_back(ValueLink(this, rightProp, false, false, false));
   }

   //////////////////////////////////////////////////////////////////////////
   bool CompareVectorAction::Update(float simDelta, float delta, int input, bool firstUpdate)
   {
      dtDAL::DataType& leftType = GetPropertyType("A");
      dtDAL::DataType& rightType = GetPropertyType("B");

      osg::Vec4 valueA;
      osg::Vec4 valueB;

      if (leftType == dtDAL::DataType::VEC2F)
      {
         osg::Vec2 vec2A = GetVec2("A");
         valueA.x() = vec2A.x();
         valueA.y() = vec2A.y();
         valueA.z() = 0.0f;
         valueA.w() = 0.0f;
      }
      else if (leftType == dtDAL::DataType::VEC3F)
      {
         osg::Vec3 vec3A = GetVec3("A");
         valueA.x() = vec3A.x();
         valueA.y() = vec3A.y();
         valueA.z() = vec3A.z();
         valueA.w() = 0.0f;
      }
      else
      {
         valueA = GetVec4("A");
      }

      if (rightType == dtDAL::DataType::VEC2F)
      {
         osg::Vec2 vec2A = GetVec2("B");
         valueB.x() = vec2A.x();
         valueB.y() = vec2A.y();
         valueB.z() = 0.0f;
         valueB.w() = 0.0f;
      }
      else if (rightType == dtDAL::DataType::VEC3F)
      {
         osg::Vec3 vec3A = GetVec3("B");
         valueB.x() = vec3A.x();
         valueB.y() = vec3A.y();
         valueB.z() = vec3A.z();
         valueB.w() = 0.0f;
      }
      else
      {
         valueB = GetVec4("B");
      }

      // Check equals exactly
      if (valueA == valueB)
      {
         OutputLink* link = GetOutputLink("A == B");
         link->Activate();
      }
      else
      {
         OutputLink* link = GetOutputLink("A != B");
         link->Activate();
      }

      // Check equivalency
      if (dtUtil::Equivalent(valueA, valueB, mEpsilon))
      {
         OutputLink* link = GetOutputLink("A equivalent to B");
         link->Activate();
      }
      else
      {
         OutputLink* link = GetOutputLink("A not equivalent to B");
         link->Activate();
      }

      return false;
   }

   //////////////////////////////////////////////////////////////////////////
   bool CompareVectorAction::CanConnectValue(ValueLink* link, ValueNode* value)
   {
      if (Node::CanConnectValue(link, value))
      {
         if (link == GetValueLink("A") || link == GetValueLink("B"))
         {
            if (value->CanBeType(dtDAL::DataType::VEC2) ||
               value->CanBeType(dtDAL::DataType::VEC3) ||
               value->CanBeType(dtDAL::DataType::VEC4))
            {
               return true;
            }
         }
         else
         {
            return true;
         }
      }

      return false;
   }

   //////////////////////////////////////////////////////////////////////////
   void CompareVectorAction::SetA(osg::Vec4 value)
   {
      mValueA = value;
   }

   //////////////////////////////////////////////////////////////////////////
   osg::Vec4 CompareVectorAction::GetA()
   {
      return mValueA;
   }

   //////////////////////////////////////////////////////////////////////////
   void CompareVectorAction::SetB(osg::Vec4 value)
   {
      mValueB = value;
   }

   //////////////////////////////////////////////////////////////////////////
   osg::Vec4 CompareVectorAction::GetB()
   {
      return mValueB;
   }

   //////////////////////////////////////////////////////////////////////////
   void CompareVectorAction::SetEpsilon(float value)
   {
      mEpsilon = value;
   }

   //////////////////////////////////////////////////////////////////////////
   float CompareVectorAction::GetEpsilon()
   {
      return mEpsilon;
   }
}

////////////////////////////////////////////////////////////////////////////////
