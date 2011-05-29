/*
 * Delta3D Open Source Game and Simulation Engine
 * Copyright (C) 2009 MOVES Institute
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
 * Author: Eric R. Heine
 */
#include <prefix/dtdirectornodesprefix.h>
#include <dtDirectorNodes/normalizevectoraction.h>

#include <dtDAL/stringactorproperty.h>

#include <dtDirector/director.h>

namespace dtDirector
{
   /////////////////////////////////////////////////////////////////////////////
   NormalizeVectorAction::NormalizeVectorAction()
      : ActionNode()
   {
      AddAuthor("Eric R. Heine");
   }

   /////////////////////////////////////////////////////////////////////////////
   NormalizeVectorAction::~NormalizeVectorAction()
   {
   }

   /////////////////////////////////////////////////////////////////////////////
   void NormalizeVectorAction::Init(const NodeType& nodeType, DirectorGraph* graph)
   {
      ActionNode::Init(nodeType, graph);
   }

   /////////////////////////////////////////////////////////////////////////////
   void NormalizeVectorAction::BuildPropertyMap()
   {
      ActionNode::BuildPropertyMap();

      // Create our value links.
      mVectorProp = new dtDAL::StringActorProperty(
         "Vector", "Vector",
         dtDAL::StringActorProperty::SetFuncType(this, &NormalizeVectorAction::SetVector),
         dtDAL::StringActorProperty::GetFuncType(this, &NormalizeVectorAction::GetVector),
         "The vector to normalize.");

      // This will expose the properties in the editor and allow
      // them to be connected to ValueNodes.
      mValues.push_back(ValueLink(this, mVectorProp.get(), true, false, false));
   }

   /////////////////////////////////////////////////////////////////////////////
   bool NormalizeVectorAction::Update(float simDelta, float delta, int input, bool firstUpdate)
   {
      dtDAL::DataType& vecType = GetPropertyType("Vector");
      if (vecType == dtDAL::DataType::VEC2F)
      {
         osg::Vec2 vec = GetVec2("Vector");
         vec.normalize();
         SetVec2(vec, "Vector");
      }
      else if (vecType == dtDAL::DataType::VEC3F)
      {
         osg::Vec3 vec = GetVec3("Vector");
         vec.normalize();
         SetVec3(vec, "Vector");
      }
      else if (vecType == dtDAL::DataType::VEC4F)
      {
         osg::Vec4 vec = GetVec4("Vector");
         vec.normalize();
         SetVec4(vec, "Vector");
      }

      return ActionNode::Update(simDelta, delta, input, firstUpdate);
   }

   ///////////////////////////////////////////////////////////////////////////////
   bool NormalizeVectorAction::CanConnectValue(ValueLink* link, ValueNode* value)
   {
      if (Node::CanConnectValue(link, value))
      {
         dtDAL::DataType& type = value->GetPropertyType();
         switch (type.GetTypeId())
         {
         case dtDAL::DataType::VEC2F_ID:
         case dtDAL::DataType::VEC3F_ID:
         case dtDAL::DataType::VEC4F_ID:
            return true;

         default:
            return false;
         }
      }

      return false;
   }

   ////////////////////////////////////////////////////////////////////////////////
   void NormalizeVectorAction::SetVector(const std::string& value)
   {
   }

   ////////////////////////////////////////////////////////////////////////////////
   std::string NormalizeVectorAction::GetVector() const
   {
      return "";
   }
}

////////////////////////////////////////////////////////////////////////////////
