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
#include <dtDirectorNodes/compareactorpropertyaction.h>

#include <dtDAL/actoridactorproperty.h>
#include <dtDAL/stringactorproperty.h>

#include <dtDirector/director.h>

namespace dtDirector
{
   /////////////////////////////////////////////////////////////////////////////
   CompareActorPropertyAction::CompareActorPropertyAction()
      : ActionNode()
   {
      mActor = "";
      mPropertyName = "";

      AddAuthor("Eric R. Heine");
   }

   /////////////////////////////////////////////////////////////////////////////
   CompareActorPropertyAction::~CompareActorPropertyAction()
   {
   }

   /////////////////////////////////////////////////////////////////////////////
   void CompareActorPropertyAction::Init(const NodeType& nodeType, DirectorGraph* graph)
   {
      ActionNode::Init(nodeType, graph);

      mOutputs.clear();
      mOutputs.push_back(OutputLink(this, "A > B"));
      mOutputs.push_back(OutputLink(this, "A == B"));
      mOutputs.push_back(OutputLink(this, "A != B"));
      mOutputs.push_back(OutputLink(this, "B > A"));
   }

   /////////////////////////////////////////////////////////////////////////////
   void CompareActorPropertyAction::BuildPropertyMap()
   {
      ActionNode::BuildPropertyMap();

      // Create our value links.
      dtDAL::ActorIDActorProperty* actorProp = new dtDAL::ActorIDActorProperty(
         "Actor", "Actor",
         dtDAL::ActorIDActorProperty::SetFuncType(this, &CompareActorPropertyAction::SetCurrentActor),
         dtDAL::ActorIDActorProperty::GetFuncType(this, &CompareActorPropertyAction::GetCurrentActor),
         "", "The actor with the property to retrieve.");
      AddProperty(actorProp);

      dtDAL::StringActorProperty* nameProp = new dtDAL::StringActorProperty(
         "PropertyName", "Property Name",
         dtDAL::StringActorProperty::SetFuncType(this, &CompareActorPropertyAction::SetPropertyName),
         dtDAL::StringActorProperty::GetFuncType(this, &CompareActorPropertyAction::GetPropertyName),
         "The name of the actor property to retrieve.");
      AddProperty(nameProp);

      dtDAL::StringActorProperty* rightProp = new dtDAL::StringActorProperty(
         "B", "B",
         dtDAL::StringActorProperty::SetFuncType(this, &CompareActorPropertyAction::SetResult),
         dtDAL::StringActorProperty::GetFuncType(this, &CompareActorPropertyAction::GetResult),
         "The value to compare against the property.");
      AddProperty(rightProp);

      // This will expose the properties in the editor and allow
      // them to be connected to ValueNodes.
      mValues.push_back(ValueLink(this, actorProp));
      mValues.push_back(ValueLink(this, nameProp, false, false, true, false));
      mValues.push_back(ValueLink(this, rightProp, false, false, false));
   }

   /////////////////////////////////////////////////////////////////////////////
   bool CompareActorPropertyAction::Update(float simDelta, float delta, int input, bool firstUpdate)
   {
      std::string valueA = "";
      std::string valueB = GetString("B");

      dtDAL::BaseActorObject* actor = GetActor("Actor");
      if (actor)
      {
         std::string propName = GetString("PropertyName");

         dtDAL::ActorProperty* prop = actor->GetProperty(propName);
         if (prop)
         {
            CompareLessThanGreaterThan(prop);
            valueA = prop->ToString();
         }
      }

      if (valueA == valueB)
      {
         GetOutputLink("A == B")->Activate();
      }
      else
      {
         GetOutputLink("A != B")->Activate();
      }

      return false;
   }

   ////////////////////////////////////////////////////////////////////////////////
   void CompareActorPropertyAction::SetPropertyName(const std::string& value)
   {
      mPropertyName = value;
   }

   ////////////////////////////////////////////////////////////////////////////////
   std::string CompareActorPropertyAction::GetPropertyName() const
   {
      return mPropertyName;
   }

   /////////////////////////////////////////////////////////////////////////////
   void CompareActorPropertyAction::SetCurrentActor(const dtCore::UniqueId& value)
   {
      mActor = value;
   }

   /////////////////////////////////////////////////////////////////////////////
   dtCore::UniqueId CompareActorPropertyAction::GetCurrentActor()
   {
      return mActor;
   }

   //////////////////////////////////////////////////////////////////////////
   void CompareActorPropertyAction::SetResult(const std::string& value)
   {
      mValueB = value;
   }

   //////////////////////////////////////////////////////////////////////////
   std::string CompareActorPropertyAction::GetResult() const
   {
      return mValueB;
   }

   ///////////////////////////////////////////////////////////////////////////////
   void CompareActorPropertyAction::CompareLessThanGreaterThan(dtDAL::ActorProperty* prop)
   {
      if (prop->GetPropertyType() == dtDAL::DataType::INT ||
         prop->GetPropertyType() == dtDAL::DataType::DOUBLE ||
         prop->GetPropertyType() == dtDAL::DataType::FLOAT)
      {
         double valueA = dtUtil::ToType<double>(prop->GetValueString());
         double valueB = GetDouble("B");
         if (valueA > valueB)
         {
            GetOutputLink("A > B")->Activate();
         }
         else if (valueA < valueB)
         {
            GetOutputLink("B > A")->Activate();
         }
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
