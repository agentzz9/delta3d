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

#include <dtDirector/nodetype.h>
#include <dtDirectorGUINodes/nodelibraryregistry.h>
#include <dtDirector/colors.h>

// Events
#include <dtDirectorGUINodes/buttonevent.h>

// Actions
#include <dtDirectorGUINodes/activatewidget.h>
#include <dtDirectorGUINodes/getwidgetproperty.h>
#include <dtDirectorGUINodes/loadguischeme.h>
#include <dtDirectorGUINodes/setguicursor.h>
#include <dtDirectorGUINodes/setlayoutvisibility.h>
#include <dtDirectorGUINodes/setwidgetproperty.h>
#include <dtDirectorGUINodes/setwidgettext.h>
#include <dtDirectorGUINodes/toggleguicursor.h>

// Values


using dtCore::RefPtr;

namespace dtDirector
{
   // Category naming convention:
   //  Core        - All Core nodes are nodes that are specifically referenced
   //                in Director and are special cases.
   //
   //  General     - General nodes provide general functionality that can be used
   //                in most, if not all, script types.
   //
   //  Value Ops   - Value Operation nodes are any nodes that perform an operation
   //                on values.
   //
   //  Conditional - Conditional nodes have multiple outputs that get triggered
   //                when a condition is met.
   //
   //  Cinematic   - Cinematic nodes are nodes that are auto-generated by the
   //                cinematic editor tool.

   // Events
   RefPtr<NodeType> NodeLibraryRegistry::BUTTON_EVENT_NODE_TYPE(         new dtDirector::NodeType(dtDirector::NodeType::EVENT_NODE,  "Button Event",          "GUI", "GUI", "React to a GUI Button Event.", NULL, Colors::ORANGE));

   // Actions
   RefPtr<NodeType> NodeLibraryRegistry::LOAD_GUI_SCHEME_NODE_TYPE(      new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Load Scheme",           "GUI", "GUI", "Load a GUI Scheme.",                     NULL, Colors::BLUE2));
   RefPtr<NodeType> NodeLibraryRegistry::SET_GUI_CURSOR_NODE_TYPE(       new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Set Cursor",            "GUI", "GUI", "Set GUI Cursor.",                        NULL, Colors::BLUE2));
   RefPtr<NodeType> NodeLibraryRegistry::SET_LAYOUT_VISIBILITY_NODE_TYPE(new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Set Layout Visibility", "GUI", "GUI", "Show or Hide a GUI Layout.",             NULL, Colors::BLUE2));
   RefPtr<NodeType> NodeLibraryRegistry::GET_WIDGET_PROPERTY_NODE_TYPE(  new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Get Widget Property",   "GUI", "GUI", "Gets a property's value from a widget.", NULL, Colors::BLUE2));
   RefPtr<NodeType> NodeLibraryRegistry::SET_WIDGET_PROPERTY_NODE_TYPE(  new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Set Widget Property",   "GUI", "GUI", "Sets a property's value on a widget.",   NULL, Colors::BLUE2));
   RefPtr<NodeType> NodeLibraryRegistry::SET_WIDGET_TEXT_NODE_TYPE(      new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Set Widget Text",       "GUI", "GUI", "Sets the text on a widget.",             NULL, Colors::BLUE2));
   RefPtr<NodeType> NodeLibraryRegistry::TOGGLE_GUI_CURSOR(              new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Toggle GUI Cursor",     "GUI", "GUI", "Shows/hides the GUI cursor.",            NULL, Colors::BLUE2));
   RefPtr<NodeType> NodeLibraryRegistry::ACTIVATE_WIDGET_TYPE(           new dtDirector::NodeType(dtDirector::NodeType::ACTION_NODE, "Activate Widget",       "GUI", "GUI", "Activates a particular widget.",         NULL, Colors::BLUE2));

   // Values

   //////////////////////////////////////////////////////////////////////////
   extern "C" GUI_NODE_LIBRARY_EXPORT dtDirector::NodePluginRegistry* CreatePluginRegistry()
   {
      return new NodeLibraryRegistry;
   }

   //////////////////////////////////////////////////////////////////////////
   extern "C" GUI_NODE_LIBRARY_EXPORT void DestroyPluginRegistry(dtDirector::NodePluginRegistry* registry)
   {
      delete registry;
   }

   //////////////////////////////////////////////////////////////////////////
   NodeLibraryRegistry::NodeLibraryRegistry()
      : dtDirector::NodePluginRegistry("dtDirectorGUINodes", "Nodes used with the dtGUI library.")
   {
   }

   //////////////////////////////////////////////////////////////////////////
   void NodeLibraryRegistry::RegisterNodeTypes()
   {
      // Events
      mNodeFactory->RegisterType<ButtonEvent>(BUTTON_EVENT_NODE_TYPE.get());

      // Actions
      mNodeFactory->RegisterType<LoadGUIScheme>(LOAD_GUI_SCHEME_NODE_TYPE.get());
      mNodeFactory->RegisterType<SetGUICursor>(SET_GUI_CURSOR_NODE_TYPE.get());
      mNodeFactory->RegisterType<SetLayoutVisibility>(SET_LAYOUT_VISIBILITY_NODE_TYPE.get());
      mNodeFactory->RegisterType<GetWidgetProperty>(GET_WIDGET_PROPERTY_NODE_TYPE.get());
      mNodeFactory->RegisterType<SetWidgetProperty>(SET_WIDGET_PROPERTY_NODE_TYPE.get());
      mNodeFactory->RegisterType<SetWidgetText>(SET_WIDGET_TEXT_NODE_TYPE.get());
      mNodeFactory->RegisterType<ToggleGUICursor>(TOGGLE_GUI_CURSOR.get());
      mNodeFactory->RegisterType<ActivateWidget>(ACTIVATE_WIDGET_TYPE.get());

      // Values
   }
}

//////////////////////////////////////////////////////////////////////////
