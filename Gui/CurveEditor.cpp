//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
*Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
*contact: immarespond at gmail dot com
*
*/

#include "CurveEditor.h"

#include <utility>

#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QHeaderView>
#include <QUndoStack> // in QtGui on Qt4, in QtWidgets on Qt5
#include <QAction>

#include "Engine/Knob.h"
#include "Engine/Curve.h"
#include "Engine/Node.h"

#include "Gui/CurveWidget.h"
#include "Gui/NodeGui.h"
#include "Gui/KnobGui.h"


using std::make_pair;
using std::cout;
using std::endl;

CurveEditor::CurveEditor(boost::shared_ptr<TimeLine> timeline, QWidget *parent)
    : QWidget(parent)
    , _nodes()
    , _mainLayout(NULL)
    , _splitter(NULL)
    , _curveWidget(NULL)
    , _tree(NULL)
    , _undoStack(new QUndoStack())
{

    _undoAction = _undoStack->createUndoAction(this,tr("&Undo"));
    _undoAction->setShortcuts(QKeySequence::Undo);
    _redoAction = _undoStack->createRedoAction(this,tr("&Redo"));
    _redoAction->setShortcuts(QKeySequence::Redo);

    _mainLayout = new QHBoxLayout(this);
    setLayout(_mainLayout);
    _mainLayout->setContentsMargins(0,0,0,0);
    _mainLayout->setSpacing(0);

    _splitter = new QSplitter(Qt::Horizontal,this);

    _curveWidget = new CurveWidget(timeline,_splitter);

    _tree = new QTreeWidget(_splitter);
    _tree->setColumnCount(1);
    _tree->header()->close();

    _splitter->addWidget(_tree);
    _splitter->addWidget(_curveWidget);


    _mainLayout->addWidget(_splitter);

    QObject::connect(_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
                     this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

}

CurveEditor::~CurveEditor(){

    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        delete (*it);
    }
    _nodes.clear();
}

std::pair<QAction*,QAction*> CurveEditor::getUndoRedoActions() const{
    return std::make_pair(_undoAction,_redoAction);
}


void CurveEditor::addNode(NodeGui* node){

    const std::vector<boost::shared_ptr<Knob> >& knobs = node->getNode()->getKnobs();
    if(knobs.empty()){
        return;
    }
    bool hasKnobsAnimating = false;
    for(U32 i = 0;i < knobs.size();++i){
        if(knobs[i]->canAnimate()){
            hasKnobsAnimating = true;
            break;
        }
    }
    if(!hasKnobsAnimating){
        return;
    }

    NodeCurveEditorContext* nodeContext = new NodeCurveEditorContext(_tree,_curveWidget,node);
    _nodes.push_back(nodeContext);

}

void CurveEditor::removeNode(NodeGui *node){
    for(std::list<NodeCurveEditorContext*>::iterator it = _nodes.begin();it!=_nodes.end();++it){
        if((*it)->getNode() == node){
            delete (*it);
            _nodes.erase(it);
            break;
        }
    }
    _curveWidget->centerOn(-10,500,-10,10);
}


NodeCurveEditorContext::NodeCurveEditorContext(QTreeWidget* tree,CurveWidget* curveWidget,NodeGui *node)
    : _node(node)
    , _nodeElements()
    , _nameItem()
{

    QTreeWidgetItem* nameItem = new QTreeWidgetItem(tree);
    nameItem->setText(0,_node->getNode()->getName().c_str());

    QObject::connect(node,SIGNAL(nameChanged(QString)),this,SLOT(onNameChanged(QString)));
    const std::map<Knob*,KnobGui*>& knobs = node->getKnobs();

    bool hasAtLeast1KnobWithACurve = false;
    bool hasAtLeast1KnobWithACurveShown = false;

    for(std::map<Knob*,KnobGui*>::const_iterator it = knobs.begin();it!=knobs.end();++it){

        Knob* k = it->first;
        KnobGui* kgui = it->second;
        if(!k->canAnimate()){
            continue;
        }

        QObject::connect(kgui,SIGNAL(keyFrameSet()),curveWidget,SLOT(updateGL()));
        QObject::connect(kgui,SIGNAL(keyFrameRemoved()),curveWidget,SLOT(updateGL()));
        QObject::connect(kgui, SIGNAL(keyInterpolationChanged()),curveWidget, SLOT(refreshDisplayedTangents()));
        
        hasAtLeast1KnobWithACurve = true;

        QTreeWidgetItem* knobItem = new QTreeWidgetItem(nameItem);

        knobItem->setText(0,k->getDescription().c_str());
        CurveGui* knobCurve = NULL;
        bool hideKnob = true;
        if(k->getDimension() == 1){
            knobCurve = curveWidget->createCurve(k->getCurve(0),k->getDescription().c_str());
            if(!k->getCurve(0)->isAnimated()){
                knobItem->setHidden(true);
            }else{
                hasAtLeast1KnobWithACurveShown = true;
                hideKnob = false;
            }
        }else{
            for(int j = 0 ; j < k->getDimension();++j){

                QTreeWidgetItem* dimItem = new QTreeWidgetItem(knobItem);
                dimItem->setText(0,k->getDimensionName(j).c_str());
                QString curveName = QString(k->getDescription().c_str()) + "." + QString(k->getDimensionName(j).c_str());
                CurveGui* dimCurve = curveWidget->createCurve(k->getCurve(j),curveName);
                NodeCurveEditorElement* elem = new NodeCurveEditorElement(tree,curveWidget,kgui,j,dimItem,dimCurve);
                QObject::connect(k,SIGNAL(restorationComplete()),elem,SLOT(checkVisibleState()));
                _nodeElements.push_back(elem);
                if(!dimCurve->getInternalCurve()->isAnimated()){
                    dimItem->setHidden(true);
                }else{
                    hasAtLeast1KnobWithACurveShown = true;
                    hideKnob = false;
                }
            }
        }



        if(hideKnob){
            knobItem->setHidden(true);
        }
        NodeCurveEditorElement* elem = new NodeCurveEditorElement(tree,curveWidget,kgui,0,knobItem,knobCurve);
        QObject::connect(k,SIGNAL(restorationComplete()),elem,SLOT(checkVisibleState()));
        _nodeElements.push_back(elem);
    }
    if(hasAtLeast1KnobWithACurve){
        NodeCurveEditorElement* elem = new NodeCurveEditorElement(tree,curveWidget,(KnobGui*)NULL,-1,
                                                                  nameItem,(CurveGui*)NULL);
        _nodeElements.push_back(elem);
        if(!hasAtLeast1KnobWithACurveShown){
            nameItem->setHidden(true);
        }
        _nameItem = nameItem;
    }


}

NodeCurveEditorContext::~NodeCurveEditorContext() {
    for(U32 i = 0 ; i < _nodeElements.size();++i){
        delete _nodeElements[i];
    }
    _nodeElements.clear();
}

void NodeCurveEditorContext::onNameChanged(const QString& name){
    _nameItem->setText(0,name);
}

static void checkIfHiddenRecursivly(QTreeWidget* tree,QTreeWidgetItem* item){
    bool areAllChildrenHidden = true;
    for(int i = 0 ;i <  item->childCount();++i){
        if(!item->child(i)->isHidden()){
            areAllChildrenHidden = false;
            break;
        }
    }
    if(areAllChildrenHidden){
        item->setHidden(true);
        item->setExpanded(false);

    }
    bool isTopLvl = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if(tree->topLevelItem(i) == item){
            isTopLvl = true;
            break;
        }
    }
    if(!isTopLvl){
        checkIfHiddenRecursivly(tree,item->parent());
    }
}



void NodeCurveEditorElement::checkVisibleState(){
    if(!_curve)
        return;
    int i = _curve->getInternalCurve()->keyFramesCount();
    if(i > 1){
        //show the item
        if(!_curveDisplayed){
            _curveDisplayed = true;
            _curve->setVisibleAndRefresh(true);
            _treeItem->setHidden(false);
            _treeItem->parent()->setHidden(false);
            _treeItem->parent()->setExpanded(true);
            if(_treeItem->parent()->parent()){
                _treeItem->parent()->parent()->setHidden(false);
                _treeItem->parent()->parent()->setExpanded(true);
            }
        }
        _treeWidget->setCurrentItem(_treeItem);
    }else{
        //hide the item
        //hiding is a bit more complex because we do not always hide the parent too,it also
        // depends on the item's siblings visibility
        if(_curveDisplayed){
            _curveDisplayed = false;
            _treeItem->setHidden(true);
            checkIfHiddenRecursivly(_treeWidget, _treeItem->parent());
            _curve->setVisibleAndRefresh(false);
        }
    }
    _knob->onInternalValueChanged(_dimension);//< also update the gui of the knob to indicate the animation is gone

}


NodeCurveEditorElement::NodeCurveEditorElement(QTreeWidget *tree, CurveWidget* curveWidget,
                                               KnobGui *knob, int dimension, QTreeWidgetItem* item, CurveGui* curve):
    _treeItem(item)
  ,_curve(curve)
  ,_curveDisplayed(false)
  ,_curveWidget(curveWidget)
  ,_treeWidget(tree)
  ,_knob(knob)
  ,_dimension(dimension)
{
    if(knob){
        QObject::connect(knob,SIGNAL(keyFrameSet()),this,SLOT(checkVisibleState()));
        QObject::connect(knob,SIGNAL(keyFrameRemoved()),this,SLOT(checkVisibleState()));
    }
    if(curve){
        if(curve->getInternalCurve()->keyFramesCount() > 1){
            _curveDisplayed = true;
        }
    }else{
        _dimension = -1; //set dimension to be meaningless
    }
}

NodeCurveEditorElement::~NodeCurveEditorElement(){
    _curveWidget->removeCurve(_curve);
    delete _treeItem;
}


void CurveEditor::centerOn(const std::vector<boost::shared_ptr<Curve> >& curves){

    // find the curve's gui
    std::vector<CurveGui*> curvesGuis;
    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        const NodeCurveEditorContext::Elements& elems = (*it)->getElements();
        for (U32 i = 0; i < elems.size(); ++i) {
            CurveGui* curve = elems[i]->getCurve();
            if (curve) {
                std::vector<boost::shared_ptr<Curve> >::const_iterator found =
                        std::find(curves.begin(), curves.end(), curve->getInternalCurve());
                if(found != curves.end()){
                    curvesGuis.push_back(curve);
                    elems[i]->getTreeItem()->setSelected(true);
                }else{
                    elems[i]->getTreeItem()->setSelected(false);
                }
            }else{
                elems[i]->getTreeItem()->setSelected(false);
            }
        }
    }
    _curveWidget->centerOn(curvesGuis);
    _curveWidget->showCurvesAndHideOthers(curvesGuis);

}


void CurveEditor::recursiveSelect(QTreeWidgetItem* cur,std::vector<CurveGui*> *curves){
    if(!cur){
        return;
    }
    cur->setSelected(true);
    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        NodeCurveEditorElement* elem = (*it)->findElement(cur);
        if(elem){
            CurveGui* curve = elem->getCurve();
            if (curve && curve->getInternalCurve()->isAnimated()) {
                curves->push_back(curve);
            }
            break;
        }

    }
    for (int j = 0; j < cur->childCount(); ++j) {
        recursiveSelect(cur->child(j),curves);
    }
}

static void recursiveDeselect(QTreeWidgetItem* current){
    current->setSelected(false);
    for (int j = 0; j < current->childCount(); ++j) {
        recursiveDeselect(current->child(j));
    }
}

void CurveEditor::onCurrentItemChanged(QTreeWidgetItem* current,QTreeWidgetItem* previous){
    std::vector<CurveGui*> curves;
    if(previous){
        recursiveDeselect(previous);
    }
    recursiveSelect(current,&curves);

    _curveWidget->showCurvesAndHideOthers(curves);
    _curveWidget->centerOn(curves); //remove this if you don't want the editor to switch to a curve on a selection change

}

NodeCurveEditorElement* NodeCurveEditorContext::findElement(CurveGui* curve){
    for(U32 i = 0; i < _nodeElements.size();++i){
        if(_nodeElements[i]->getCurve() == curve){
            return _nodeElements[i];
        }
    }
    return NULL;
}

NodeCurveEditorElement* NodeCurveEditorContext::findElement(KnobGui* knob,int dimension){
    for(U32 i = 0; i < _nodeElements.size();++i){
        if(_nodeElements[i]->getKnob() == knob && _nodeElements[i]->getDimension() == dimension){
            return _nodeElements[i];
        }
    }
    return NULL;
}

NodeCurveEditorElement* NodeCurveEditorContext::findElement(QTreeWidgetItem* item) {
    for (U32 i = 0; i < _nodeElements.size(); ++i) {
        if (_nodeElements[i]->getTreeItem() == item) {
            return _nodeElements[i];
        }
    }
    return NULL;
}


namespace { // protect local classes in anonymous namespace

struct NewKeyFrame{
    boost::shared_ptr<KeyFrame> key;
    NodeCurveEditorElement* element;
    SequenceTime time;
    Variant value;
};

class AddKeyCommand : public QUndoCommand{
public:

    AddKeyCommand(CurveWidget *editor, NodeCurveEditorElement *curveEditorElement, const std::string& actionName,
                  SequenceTime time, const Variant& value, QUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();

private:

    std::string _actionName;
    NewKeyFrame _key;
    CurveWidget *_editor;
};

class PasteKeysCommand : public QUndoCommand{
public:

    PasteKeysCommand(CurveWidget *editor, std::vector<NodeCurveEditorElement *> elements
                     , const std::vector<std::pair<SequenceTime, Variant> > &keys, QUndoCommand *parent = 0);

    virtual ~PasteKeysCommand() { _keys.clear() ;}
    virtual void undo();
    virtual void redo();
private:

    std::string _actionName;
    std::vector<boost::shared_ptr<NewKeyFrame> > _keys;
    CurveWidget *_editor;
};

class RemoveMultipleKeysCommand : public QUndoCommand{
public:
    RemoveMultipleKeysCommand(CurveWidget* editor,const std::vector<NodeCurveEditorElement*>& curveEditorElement
                              ,const std::vector<boost::shared_ptr<KeyFrame> >& key,QUndoCommand *parent = 0);
    virtual ~RemoveMultipleKeysCommand() { _keys.clear(); }
    virtual void undo();
    virtual void redo();

private:

    std::vector<std::pair<NodeCurveEditorElement*,boost::shared_ptr<KeyFrame> > > _keys;
    CurveWidget* _curveWidget;
};

class RemoveKeyCommand : public QUndoCommand{
public:

    RemoveKeyCommand(CurveWidget* editor,NodeCurveEditorElement* curveEditorElement
                     ,boost::shared_ptr<KeyFrame> key,QUndoCommand *parent = 0);
    virtual void undo();
    virtual void redo();

private:

    NodeCurveEditorElement* _element;
    boost::shared_ptr<KeyFrame> _key;
    CurveWidget* _curveWidget;
};

class MoveKeyCommand : public QUndoCommand{

public:

    MoveKeyCommand(CurveWidget* editor,CurveGui* curve,boost::shared_ptr<KeyFrame> key,double oldx,const Variant& oldy,
                   double newx,const Variant& newy,
                   QUndoCommand *parent = 0);
    virtual void undo();
    virtual void redo();
    virtual int id() const { return kCurveEditorMoveKeyCommandCompressionID; }
    virtual bool mergeWith(const QUndoCommand * command);

private:

    CurveGui* _curve;
    double _newX,_oldX;
    Variant _newY,_oldY;
    boost::shared_ptr<KeyFrame> _key;
    CurveWidget* _curveWidget;
};



class MoveMultipleKeysCommand : public QUndoCommand{

    struct KeyMove{
        CurveGui* _curve;
        KnobGui* _knob;
        boost::shared_ptr<KeyFrame> _key;
        double _oldX,_newX;
        Variant _oldY,_newY;
    };

public:

    MoveMultipleKeysCommand(CurveWidget* editor,
                            const std::vector<std::pair<KnobGui*,CurveGui*> >& knobs,
                            const std::vector< std::pair< boost::shared_ptr<KeyFrame> , std::pair<double,Variant> > >& keys
                            ,QUndoCommand *parent = 0);
    virtual ~MoveMultipleKeysCommand(){ _keys.clear(); }
    virtual void undo();
    virtual void redo();
    virtual int id() const { return kCurveEditorMoveMultipleKeysCommandCompressionID; }
    virtual bool mergeWith(const QUndoCommand * command);

private:

    std::vector< boost::shared_ptr<KeyMove> > _keys;
    CurveWidget* _curveWidget;
};



class SetKeyInterpolationCommand : public QUndoCommand{

public:

    SetKeyInterpolationCommand(CurveWidget* editor,Natron::KeyframeType newInterp,
                               CurveGui* curve,
                               boost::shared_ptr<KeyFrame> key,
                               QUndoCommand *parent = 0);
    virtual void undo();
    virtual void redo();

private:

    CurveGui* _curve;
    Natron::KeyframeType _oldInterp;
    Natron::KeyframeType _newInterp;
    boost::shared_ptr<KeyFrame> _key;
    CurveWidget* _curveWidget;
};

struct KeyInterpolationChange{
    Natron::KeyframeType oldInterp;
    Natron::KeyframeType newInterp;
    CurveGui* curve;
    boost::shared_ptr<KeyFrame> key;
    KnobGui* knob;
};
    
class SetMultipleKeysInterpolationCommand : public QUndoCommand{

public:

    SetMultipleKeysInterpolationCommand(CurveWidget* editor, Natron::KeyframeType newInterp,
                                        const std::vector<std::pair<KnobGui*,std::pair<CurveGui*,boost::shared_ptr<KeyFrame> > > >& keys,
                               QUndoCommand *parent = 0);
    virtual void undo();
    virtual void redo();

private:

    std::vector< boost::shared_ptr< KeyInterpolationChange > > _oldInterp;
    CurveWidget* _curveWidget;
};

} // end of anonymous namespace


CurveGui* CurveEditor::findCurve(KnobGui* knob,int dimension){
    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        NodeCurveEditorElement* elem = (*it)->findElement(knob,dimension);
        if(elem){
            return elem->getCurve();
        }
    }
    return (CurveGui*)NULL;
}

void CurveEditor::addKeyFrame(KnobGui* knob,SequenceTime time,int dimension){
    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        NodeCurveEditorElement* elem = (*it)->findElement(knob,dimension);
        if(elem){
            std::string actionName(knob->getKnob()->getDescription()+knob->getKnob()->getDimensionName(dimension));
            _undoStack->push(new AddKeyCommand(_curveWidget,elem,actionName,time,knob->getKnob()->getValue(dimension)));
            return;
        }
    }
}

void CurveEditor::addKeyFrame(CurveGui* curve, SequenceTime time, const Variant& value){
    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        NodeCurveEditorElement* elem = (*it)->findElement(curve);
        if(elem){
            std::string actionName(elem->getKnob()->getKnob()->getDescription()
                                   + "." + elem->getKnob()->getKnob()->getDimensionName(elem->getDimension()));

            _undoStack->push(new AddKeyCommand(_curveWidget,elem,actionName,time,value));
            return;
        }
    }
}

void CurveEditor::addKeyFrames(CurveGui* curve,const std::vector<std::pair<SequenceTime,Variant> >& keys){

    std::vector<NodeCurveEditorElement*> elements;
    NodeCurveEditorElement* elem = NULL;
    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        elem = (*it)->findElement(curve);
        if(elem){
            break;
        }
    }


    for(U32 i = 0 ; i < keys.size();++i){
        elements.push_back(elem);
    }
    _undoStack->push(new PasteKeysCommand(_curveWidget,elements,keys));

}

AddKeyCommand::AddKeyCommand(CurveWidget *editor,  NodeCurveEditorElement *curveEditorElement, const std::string &actionName, SequenceTime time,
                             const Variant &value, QUndoCommand *parent)
    : QUndoCommand(parent)
    , _actionName(actionName)
    , _key()
    , _editor(editor)
{
    _key.element = curveEditorElement;
    _key.time = time;
    _key.value = value;
}

void AddKeyCommand::undo(){


    CurveGui* curve = _key.element->getCurve();
    assert(curve);
    _editor->removeKeyFrame(curve,_key.key);
    _key.element->checkVisibleState();
    _editor->updateGL();
    setText(QObject::tr("Add keyframe to %1")
            .arg(_actionName.c_str()));

}
void AddKeyCommand::redo(){
    CurveGui* curve = _key.element->getCurve();
    if(!_key.key){
        assert(curve);
        _key.key = _editor->addKeyFrame(curve, _key.value, _key.time);
    }else{
        _editor->addKeyFrame(curve, _key.key);
    }
    _key.element->checkVisibleState();
    _editor->updateGL();

    setText(QObject::tr("Add keyframe to %1")
            .arg(_actionName.c_str()));

}

PasteKeysCommand::PasteKeysCommand(CurveWidget *editor, std::vector<NodeCurveEditorElement *> elements
                                   ,const std::vector<std::pair<SequenceTime,Variant> >& keys, QUndoCommand *parent)
    : QUndoCommand(parent)
    , _keys()
    , _editor(editor)
{
    assert(elements.size() == keys.size());
    for(U32 i = 0; i < elements.size();++i){
        boost::shared_ptr<NewKeyFrame> newKey(new NewKeyFrame());
        newKey->element = elements[i];
        newKey->time = keys[i].first;
        newKey->value = keys[i].second;
        _keys.push_back(newKey);
    }

}

void PasteKeysCommand::undo(){
    for(U32 i = 0; i < _keys.size();++i){
        CurveGui* curve = _keys[i]->element->getCurve();
        assert(curve);
        _editor->removeKeyFrame(curve,_keys[i]->key);
        _keys[i]->element->checkVisibleState();
    }
    _editor->updateGL();

    setText(QObject::tr("Add multiple keyframes"));
}

void PasteKeysCommand::redo(){
    for(U32 i = 0; i < _keys.size();++i){
        CurveGui* curve = _keys[i]->element->getCurve();
        if (!_keys[i]->key) {
            assert(curve);
            _keys[i]->key = _editor->addKeyFrame(curve,_keys[i]->value,_keys[i]->time);
        } else {
            _editor->addKeyFrame(curve,_keys[i]->key);
        }
        _keys[i]->element->checkVisibleState();
    }
    _editor->updateGL();
    setText(QObject::tr("Add multiple keyframes"));

}

void CurveEditor::removeKeyFrame(CurveGui* curve,boost::shared_ptr<KeyFrame> key){
    for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
        it!=_nodes.end();++it){
        NodeCurveEditorElement* elem = (*it)->findElement(curve);
        if(elem){
            _undoStack->push(new RemoveKeyCommand(_curveWidget,elem,key));
            return;
        }
    }
}

void CurveEditor::removeKeyFrames(const std::vector< std::pair<CurveGui *,boost::shared_ptr<KeyFrame> > > &keys){
    if(keys.empty()){
        return;
    }
    std::vector<NodeCurveEditorElement*> elements;
    std::vector<boost::shared_ptr<KeyFrame> > keyframes;
    for(U32 i = 0 ; i< keys.size() ;++i){
        for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
            it!=_nodes.end();++it){
            NodeCurveEditorElement* elem = (*it)->findElement(keys[i].first);
            if(elem){
                elements.push_back(elem);
                keyframes.push_back(keys[i].second);
                break;
            }
        }
    }
    if(!elements.empty()){
        _undoStack->push(new RemoveMultipleKeysCommand(_curveWidget,elements,keyframes));
    }
}

RemoveKeyCommand::RemoveKeyCommand(CurveWidget *editor, NodeCurveEditorElement *curveEditorElement, boost::shared_ptr<KeyFrame> key, QUndoCommand *parent)
    : QUndoCommand(parent)
    , _element(curveEditorElement)
    , _key(key)
    , _curveWidget(editor)
{

}


void RemoveKeyCommand::undo(){
    assert(_key);
    _curveWidget->addKeyFrame(_element->getCurve(),_key);
    _element->checkVisibleState();
    _curveWidget->updateGL();
    setText(QObject::tr("Remove keyframe from %1.%2")
            .arg(_element->getKnob()->getKnob()->getDescription().c_str())
            .arg(_element->getKnob()->getKnob()->getDimensionName(_element->getDimension()).c_str()));


}
void RemoveKeyCommand::redo(){
    assert(_key);
    _curveWidget->removeKeyFrame(_element->getCurve(),_key);
    _element->checkVisibleState();
    _curveWidget->updateGL();

    setText(QObject::tr("Remove keyframe from %1.%2")
            .arg(_element->getKnob()->getKnob()->getDescription().c_str())
            .arg(_element->getKnob()->getKnob()->getDimensionName(_element->getDimension()).c_str()));

}

RemoveMultipleKeysCommand::RemoveMultipleKeysCommand(CurveWidget* editor,const std::vector<NodeCurveEditorElement*>& curveEditorElement
                                                     ,const std::vector<boost::shared_ptr<KeyFrame> >& key,QUndoCommand *parent )
    : QUndoCommand(parent)
    , _keys()
    , _curveWidget(editor)
{
    assert(curveEditorElement.size() == key.size());
    for(U32 i = 0 ; i < curveEditorElement.size();++i){
        _keys.push_back(std::make_pair(curveEditorElement[i],key[i]));
    }
}
void RemoveMultipleKeysCommand::undo(){
    for(U32 i = 0 ; i < _keys.size();++i){
        assert(_keys[i].second);
        _curveWidget->addKeyFrame(_keys[i].first->getCurve(),_keys[i].second);
        _keys[i].first->checkVisibleState();
    }
     _curveWidget->updateGL();
    setText(QObject::tr("Remove multiple keyframes"));



}
void RemoveMultipleKeysCommand::redo(){
    for(U32 i = 0 ; i < _keys.size();++i){
        assert(_keys[i].second);
        _curveWidget->removeKeyFrame(_keys[i].first->getCurve(),_keys[i].second);
        _keys[i].first->checkVisibleState();
    }
     _curveWidget->updateGL();
    setText(QObject::tr("Remove multiple keyframes"));;

}


void CurveEditor::setKeyFrame(CurveGui* curve,boost::shared_ptr<KeyFrame> key,double x,const Variant& y){
    _undoStack->push(new MoveKeyCommand(_curveWidget,curve,key,key->getTime(),key->getValue(), x,y));
}


MoveKeyCommand::MoveKeyCommand(CurveWidget* editor,CurveGui* curve, boost::shared_ptr<KeyFrame> key,
                               double oldx, const Variant &oldy, double newx, const Variant &newy, QUndoCommand *parent)
    : QUndoCommand(parent)
    , _curve(curve)
    , _newX(newx)
    , _oldX(oldx)
    , _newY(newy)
    , _oldY(oldy)
    , _key(key)
    , _curveWidget(editor)
{

}
void MoveKeyCommand::undo(){
    assert(_key);
    _curve->getInternalCurve()->setKeyFrameValueAndTime(_oldX, _oldY, _key);
    _curveWidget->refreshDisplayedTangents();
    setText(QObject::tr("Move keyframe"));
}
void MoveKeyCommand::redo(){
    assert(_key);
    _curve->getInternalCurve()->setKeyFrameValueAndTime(_newX, _newY, _key);
    _curveWidget->refreshDisplayedTangents();
    setText(QObject::tr("Move keyframe"));
}
bool MoveKeyCommand::mergeWith(const QUndoCommand * command){
    const MoveKeyCommand* cmd = dynamic_cast<const MoveKeyCommand*>(command);
    if(cmd && cmd->id() == id()){
        _newX = cmd->_newX;
        _newY = cmd->_newY;
        return true;
    }else{
        return false;
    }
}

void CurveEditor::setKeyFrames(const std::vector<std::pair< std::pair<CurveGui*,boost::shared_ptr<KeyFrame> >, std::pair<double, Variant> > > &keys){
    std::vector<std::pair<KnobGui*,CurveGui*> > knobs;
    std::vector< std::pair< boost::shared_ptr<KeyFrame> , std::pair<double,Variant> > > moves;
    for(U32 i = 0 ; i< keys.size() ;++i){
        for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
            it!=_nodes.end();++it){
            NodeCurveEditorElement* elem = (*it)->findElement(keys[i].first.first);
            if(elem){
                knobs.push_back(std::make_pair(elem->getKnob(),keys[i].first.first));
                moves.push_back(std::make_pair(keys[i].first.second,keys[i].second));
                break;
            }
        }

    }
    _undoStack->push(new MoveMultipleKeysCommand(_curveWidget,knobs,moves));
}
MoveMultipleKeysCommand::MoveMultipleKeysCommand(CurveWidget* editor,
                                                 const std::vector<std::pair<KnobGui*,CurveGui*> >& knobs,
                                                 const std::vector< std::pair< boost::shared_ptr<KeyFrame> , std::pair<double,Variant> > >& keys
                                                 ,QUndoCommand *parent )
    : QUndoCommand(parent)
    , _keys()
    , _curveWidget(editor)
{
    assert(knobs.size() == keys.size());
    for(U32 i = 0; i < keys.size();++i){
        boost::shared_ptr<KeyMove> move(new KeyMove());
        move->_curve = knobs[i].second;
        move->_knob = knobs[i].first;
        move->_key = keys[i].first;
        move->_oldX = move->_key->getTime();
        move->_newX = keys[i].second.first;
        move->_oldY = move->_key->getValue();
        move->_newY = keys[i].second.second;
        _keys.push_back(move);
    }

}
void MoveMultipleKeysCommand::undo(){
    for(U32 i = 0; i < _keys.size();++i){
        _keys[i]->_knob->getKnob()->beginValueChange(Natron::USER_EDITED);

    }
    for(U32 i = 0; i < _keys.size();++i){
        assert(_keys[i]->_key);
        _keys[i]->_curve->getInternalCurve()->setKeyFrameValueAndTime(_keys[i]->_oldX, _keys[i]->_oldY, _keys[i]->_key);
    }
    for(U32 i = 0; i < _keys.size();++i){
        _keys[i]->_knob->getKnob()->endValueChange(Natron::USER_EDITED);

    }
    _curveWidget->refreshSelectedKeysBbox();
    _curveWidget->refreshDisplayedTangents();
    setText(QObject::tr("Move multiple keys"));
}
void MoveMultipleKeysCommand::redo(){
    for(U32 i = 0; i < _keys.size();++i){
        _keys[i]->_knob->getKnob()->beginValueChange(Natron::USER_EDITED);

    }
    for(U32 i = 0; i < _keys.size();++i){
        assert(_keys[i]->_key);
        _keys[i]->_curve->getInternalCurve()->setKeyFrameValueAndTime(_keys[i]->_newX, _keys[i]->_newY, _keys[i]->_key);
    }
    for(U32 i = 0; i < _keys.size();++i){
        _keys[i]->_knob->getKnob()->endValueChange(Natron::USER_EDITED);

    }
    _curveWidget->refreshSelectedKeysBbox();
    _curveWidget->refreshDisplayedTangents();

    setText(QObject::tr("Move multiple keys"));
}
bool MoveMultipleKeysCommand::mergeWith(const QUndoCommand * command){
    const MoveMultipleKeysCommand* cmd = dynamic_cast<const MoveMultipleKeysCommand*>(command);
    if(cmd && cmd->id() == id()){
        if(_keys.size() != cmd->_keys.size()){
            return false;
        }
        for(U32 i = 0; i < _keys.size();++i){
            _keys[i]->_newX = cmd->_keys[i]->_newX;
            _keys[i]->_newY = cmd->_keys[i]->_newY;
        }
        return true;
    }else{
        return false;
    }
}

void CurveEditor::setKeyInterpolation(CurveGui* curve,boost::shared_ptr<KeyFrame> key,Natron::KeyframeType interp){
    _undoStack->push(new SetKeyInterpolationCommand(_curveWidget,interp,curve,key));
}

void CurveEditor::setKeysInterpolation(const std::vector<std::pair<CurveGui*,boost::shared_ptr<KeyFrame> > >& keys,Natron::KeyframeType interp){
    std::vector<std::pair<KnobGui*,std::pair<CurveGui*,boost::shared_ptr<KeyFrame> > > > keyChanges;
    for(U32 i = 0 ; i< keys.size() ;++i){
        for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
            it!=_nodes.end();++it){
            NodeCurveEditorElement* elem = (*it)->findElement(keys[i].first);
            if(elem){
                keyChanges.push_back(std::make_pair(elem->getKnob(),std::make_pair(keys[i].first,keys[i].second)));
                break;
            }
        }
        
    }
    _undoStack->push(new SetMultipleKeysInterpolationCommand(_curveWidget,interp,keyChanges));
}


SetKeyInterpolationCommand::SetKeyInterpolationCommand(CurveWidget* editor,Natron::KeyframeType newInterp,
                                                       CurveGui* curve,
                                                       boost::shared_ptr<KeyFrame> key,
                                                       QUndoCommand *parent)
    : QUndoCommand(parent)
    , _curve(curve)
    , _oldInterp(key->getInterpolation())
    , _newInterp(newInterp)
    , _key(key)
    , _curveWidget(editor)
{

}

void SetKeyInterpolationCommand::undo(){
    _curve->getInternalCurve()->setKeyFrameInterpolation(_oldInterp, _key);
    _curveWidget->refreshDisplayedTangents();
    setText(QObject::tr("Set key interpolation"));
}

void SetKeyInterpolationCommand::redo(){
    _curve->getInternalCurve()->setKeyFrameInterpolation(_newInterp, _key);
    _curveWidget->refreshDisplayedTangents();
    setText(QObject::tr("Set key interpolation"));
}

SetMultipleKeysInterpolationCommand::SetMultipleKeysInterpolationCommand(CurveWidget* editor,Natron::KeyframeType newInterp,
                        const std::vector<std::pair<KnobGui*,std::pair<CurveGui*,boost::shared_ptr<KeyFrame> > > >& keys,
                        QUndoCommand *parent)
    : QUndoCommand(parent)
    , _oldInterp()
    , _curveWidget(editor)
{
    for(U32 i = 0; i < keys.size();++i){
        boost::shared_ptr<KeyInterpolationChange> keyChange(new KeyInterpolationChange);
        keyChange->curve = keys[i].second.first;
        keyChange->key = keys[i].second.second;
        keyChange->knob= keys[i].first;
        keyChange->oldInterp = keys[i].second.second->getInterpolation();
        keyChange->newInterp = newInterp;
        _oldInterp.push_back(keyChange);
    }
}

void SetMultipleKeysInterpolationCommand::undo(){
    for (U32 i = 0; i < _oldInterp.size();++i) {
        _oldInterp[i]->knob->getKnob()->beginValueChange(Natron::USER_EDITED);
    }
     for(U32 i = 0; i < _oldInterp.size();++i){
         _oldInterp[i]->curve->getInternalCurve()->setKeyFrameInterpolation(_oldInterp[i]->oldInterp, _oldInterp[i]->key);
     }
    for (U32 i = 0; i < _oldInterp.size();++i) {
        _oldInterp[i]->knob->getKnob()->endValueChange(Natron::USER_EDITED);
    }
    _curveWidget->refreshDisplayedTangents();
    setText(QObject::tr("Set multiple keys interpolation"));
}

void SetMultipleKeysInterpolationCommand::redo(){
    for (U32 i = 0; i < _oldInterp.size();++i) {
        _oldInterp[i]->knob->getKnob()->beginValueChange(Natron::USER_EDITED);
    }
    for(U32 i = 0; i < _oldInterp.size();++i){
       _oldInterp[i]->curve->getInternalCurve()->setKeyFrameInterpolation(_oldInterp[i]->newInterp, _oldInterp[i]->key);
    }
    for (U32 i = 0; i < _oldInterp.size();++i) {
        _oldInterp[i]->knob->getKnob()->endValueChange(Natron::USER_EDITED);
    }

    _curveWidget->refreshDisplayedTangents();
    setText(QObject::tr("Set multiple keys interpolation"));
}


void CurveEditor::hideCurves(KnobGui* knob){
    for(int i = 0 ; i < knob->getKnob()->getDimension();++i){
        for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
            it!=_nodes.end();++it){
            NodeCurveEditorElement* elem = (*it)->findElement(knob,i);
            if(elem){
                elem->getCurve()->setVisible(false);
                elem->getTreeItem()->setHidden(true);
                checkIfHiddenRecursivly(_tree, elem->getTreeItem());
                break;
            }
        }
    }
    _curveWidget->updateGL();
}

void CurveEditor::showCurves(KnobGui* knob){
    for(int i = 0 ; i < knob->getKnob()->getDimension();++i){
        for(std::list<NodeCurveEditorContext*>::const_iterator it = _nodes.begin();
            it!=_nodes.end();++it){
            NodeCurveEditorElement* elem = (*it)->findElement(knob,i);
            if(elem){
                if(elem->getCurve()->getInternalCurve()->isAnimated()){
                    elem->getCurve()->setVisible(true);
                    elem->getTreeItem()->setHidden(false);
                    if(elem->getTreeItem()->parent()){
                        elem->getTreeItem()->parent()->setHidden(false);
                        elem->getTreeItem()->parent()->setExpanded(true);
                        if(elem->getTreeItem()->parent()->parent()){
                            elem->getTreeItem()->parent()->parent()->setHidden(false);
                            elem->getTreeItem()->parent()->parent()->setExpanded(true);
                        }
                    }
                }
                break;
            }
        }
    }
     _curveWidget->updateGL();

}
