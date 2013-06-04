//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com
#ifndef OPERATORNODE_UI_H
#define OPERATORNODE_UI_H
#include "Gui/node_ui.h"

class OperatorNode_ui : public NodeGui
{
public:
    OperatorNode_ui(std::vector<NodeGui*> nodes,QVBoxLayout* dockContainer,
                    Node *node,qreal x, qreal y,QGraphicsItem* parent=0,QGraphicsScene* sc=0);
     bool hasOutput(){return true;}
     void initInputArrows(){NodeGui::initInputArrows();}
     QRectF boundingRect() const{return NodeGui::boundingRect();}
     void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *parent){
         NodeGui::paint(painter,options,parent);
     }
};

#endif // OPERATORNODE_UI_H
