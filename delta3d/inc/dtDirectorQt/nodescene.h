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
 * Author: Eric R. Heine
 */

#ifndef NODE_SCENE
#define NODE_SCENE

#include <dtDirectorQt/export.h>
#include <dtDirector/nodetype.h>

#include <QtGui/QGraphicsScene>

namespace dtDirector
{
   class DirectorEditor;
   class DirectorGraph;
   class NodeItem;

   /**
   * @class EditorScene
   */
   class DT_DIRECTOR_QT_EXPORT NodeScene : public QGraphicsScene
   {
      Q_OBJECT

   public:

      /**
      * Constructor.
      *
      * @param[in] parent The parent editor.
      */
      NodeScene(DirectorEditor* parent);

      /**
       * Refresh the scene to display newly loaded nodes
       *
       * @param nodeType The type of nodes to display in the scene
       */
      void RefreshNodes(NodeType::NodeTypeEnum nodeType);

   signals:
      void CreateNode(const QString& name, const QString& category);

   protected:
      virtual void dragMoveEvent(QGraphicsSceneDragDropEvent* event);
      virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* mouseEvent);
      virtual void mousePressEvent(QGraphicsSceneMouseEvent* mouseEvent);

   private:
      /**
       * Creates a new node.
       *
       * @param[in]     name      The name of the node.
       * @param[in]     category  The category of the node.
       * @param[in,out] x, y      Starting UI coordinates to spawn the node.
       * @param[in,out] maxWidth  The maximum width of all nodes in this scene.
       */
      void CreateNode(NodeType::NodeTypeEnum nodeType, const std::string& name,
         const std::string& category, float x, float& y, float& maxWidth);

      /**
       * Gets the NodeItem at the given position
       *
       * @param[in]  pos  The position to find a NodeItem from
       *
       * @return The NodeItem at the given position
       */
      NodeItem* GetNodeItemAtPos(const QPointF& pos);

      DirectorEditor* mpEditor;
      DirectorGraph* mpGraph;
      QGraphicsRectItem* mpTranslationItem;
      NodeItem* mpDraggedItem;
   };
} // namespace dtDirector

#endif // NODE_SCENE
