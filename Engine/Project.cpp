//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */

#include "Project.h"

#include <fstream>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>


#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/archive_exception.hpp>

#include "Global/AppManager.h"

#include "Engine/TimeLine.h"
#include "Engine/Node.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/Knob.h"
#include "Engine/ViewerInstance.h"
#include "Engine/VideoEngine.h"
#include "Engine/CurveSerialization.h"

#include "Gui/NodeGui.h"
#include "Gui/ViewerTab.h"
#include "Gui/ViewerGL.h"
#include "Gui/Gui.h"



using namespace Natron;
using std::cout; using std::endl;
using std::make_pair;


static QString generateStringFromFormat(const Format& f){
    QString formatStr;
    formatStr.append(f.getName().c_str());
    formatStr.append("  ");
    formatStr.append(QString::number(f.width()));
    formatStr.append(" x ");
    formatStr.append(QString::number(f.height()));
    formatStr.append("  ");
    formatStr.append(QString::number(f.getPixelAspect()));
    return formatStr;
}

Project::Project(AppInstance* appInstance):
KnobHolder(appInstance)
, _projectName("Untitled." NATRON_PROJECT_FILE_EXT)
, _hasProjectBeenSavedByUser(false)
, _ageSinceLastSave(QDateTime::currentDateTime())
, _formatKnob(NULL)
, _addFormatKnob(NULL)
, _viewsCount(NULL)
, _timeline(new TimeLine())
, _autoSetProjectFormat(true)
, _projectDataLock()
, _currentNodes()
, _availableFormats()
, _knobsAge(0)
{
}

Project::~Project(){
    QMutexLocker locker(&_projectDataLock);
    for (U32 i = 0; i < _currentNodes.size(); ++i) {
        if(_currentNodes[i]->isOutputNode()){
            dynamic_cast<OutputEffectInstance*>(_currentNodes[i]->getLiveInstance())->getVideoEngine()->quitEngineThread();
        }
    }
    for (U32 i = 0; i < _currentNodes.size(); ++i) {
        delete _currentNodes[i];
    }
    _currentNodes.clear();
}

void Project::initializeKnobs(){
    _formatKnob = dynamic_cast<ComboBox_Knob*>(appPTR->getKnobFactory().createKnob("ComboBox", this, "Output Format"));
    const std::vector<Format*>& appFormats = appPTR->getFormats();
    std::vector<std::string> entries;
    for (U32 i = 0; i < appFormats.size(); ++i) {
        Format* f = appFormats[i];
        QString formatStr = generateStringFromFormat(*f);
        if(f->width() == 1920 && f->height() == 1080){
            _formatKnob->setValue(i);
        }
        entries.push_back(formatStr.toStdString());
        _availableFormats.push_back(*f);
    }
    
    _formatKnob->populate(entries);
    
    _addFormatKnob = dynamic_cast<Button_Knob*>(appPTR->getKnobFactory().createKnob("Button",this,"New format..."));
    
    
    _viewsCount = dynamic_cast<Int_Knob*>(appPTR->getKnobFactory().createKnob("Int",this,"Number of views"));
    _viewsCount->turnOffAnimation();
    _viewsCount->setMinimum(1);
    _viewsCount->setValue(1);
    _viewsCount->disableSlider();
}


void Project::evaluate(Knob* knob,bool /*isSignificant*/){
    if(knob == _viewsCount){
        int viewsCount = _viewsCount->getValue<int>();
        getApp()->setupViewersForViews(viewsCount);
    }else if(knob == _formatKnob){
        const Format& f = _availableFormats[_formatKnob->getActiveEntry()];
        for(U32 i = 0 ; i < _currentNodes.size() ; ++i){
            if (_currentNodes[i]->pluginID() == "Viewer") {
                ViewerInstance* n = dynamic_cast<ViewerInstance*>(_currentNodes[i]->getLiveInstance());
                assert(n);
                n->getUiContext()->viewer->onProjectFormatChanged(f);
                n->refreshAndContinueRender();
            }
        }
    }else if(knob == _addFormatKnob){
        createNewFormat();
    }
}


const Format& Project::getProjectDefaultFormat() const{
    int index = _formatKnob->getActiveEntry();
    return _availableFormats[index];
}

void Project::initNodeCountersAndSetName(Node* n){
    assert(n);
    std::map<std::string,int>::iterator it = _nodeCounters.find(n->pluginID());
    if(it != _nodeCounters.end()){
        it->second++;
        n->setName(QString(QString(n->pluginLabel().c_str())+ "_" + QString::number(it->second)).toStdString());
    }else{
        _nodeCounters.insert(make_pair(n->pluginID(), 1));
        n->setName(QString(QString(n->pluginLabel().c_str())+ "_" + QString::number(1)).toStdString());
    }
    _currentNodes.push_back(n);
}

void Project::clearNodes(){
    foreach(Node* n,_currentNodes){
        delete n;
    }
    _currentNodes.clear();
}

void Project::setFrameRange(int first, int last){
    _timeline->setFrameRange(first,last);
}

void Project::seekFrame(int frame){
    _timeline->seekFrame(frame);
}

void Project::incrementCurrentFrame() {
    _timeline->incrementCurrentFrame();
}
void Project::decrementCurrentFrame(){
    _timeline->decrementCurrentFrame();
}

int Project::currentFrame() const {
    return _timeline->currentFrame();
}

int Project::firstFrame() const {
    return _timeline->firstFrame();
}

int Project::lastFrame() const {
    return _timeline->lastFrame();
}


void Project::loadProject(const QString& path,const QString& name,bool background){
    QString filePath = path+name;
    if(!QFile::exists(filePath)){
        throw std::invalid_argument(QString(filePath + " : no such file.").toStdString());
    }
    std::list<NodeGui::SerializedState> nodeStates;
    std::ifstream ifile;
    try{
        ifile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        ifile.open(filePath.toStdString().c_str(),std::ifstream::in);
    }catch(const std::ifstream::failure& e){
        throw std::runtime_error(std::string(std::string("Exception opening ")+ e.what() + filePath.toStdString()));
    }
    try{
        boost::archive::xml_iarchive iArchive(ifile);
        iArchive >> boost::serialization::make_nvp("Nodes",nodeStates);
        iArchive >> boost::serialization::make_nvp("Project_formats",_availableFormats);
        /*we must restore the entries in the combobox before restoring the value*/
        std::vector<std::string> entries;
        for (U32 i = 0; i < _availableFormats.size(); ++i) {
            QString formatStr = generateStringFromFormat(_availableFormats[i]);
            entries.push_back(formatStr.toStdString());
        }
        _formatKnob->populate(entries);
        
        AnimatingParam formatValue(_formatKnob->getDimension());
        AnimatingParam viewsValue(_viewsCount->getDimension());
        iArchive >> boost::serialization::make_nvp("Project_output_format",formatValue);
        _formatKnob->onStartupRestoration(formatValue);
        setAutoSetProjectFormat(false);
        iArchive >> boost::serialization::make_nvp("Project_views_count",viewsValue);
        _viewsCount->onStartupRestoration(viewsValue);
        ifile.close();
    }catch(const boost::archive::archive_exception& e){
        throw std::runtime_error(std::string("Serialization error: ") + std::string(e.what()));
    }
    
    bool hasProjectAWriter = false;
    /*first create all nodes and restore the knobs values*/
    for (std::list<NodeGui::SerializedState>::const_iterator it = nodeStates.begin() ; it!=nodeStates.end(); ++it) {
        const NodeGui::SerializedState& state = *it;
        
        if (background && state.getClassName() == "Viewer") { //if the node is a viewer, don't try to load it in background mode
            continue;
        }
        
        Node* n = getApp()->createNode(state.getClassName().c_str(),true);
        n->getLiveInstance()->beginValuesChanged(AnimatingParam::PLUGIN_EDITED,true);
        if(n->pluginID() == "Writer" || (n->isOpenFXNode() && n->isOutputNode())){
            hasProjectAWriter = true;
        }
        
        if(!n){
            clearNodes();
            QString text("Failed to restore the graph! \n The node ");
            text.append(state.getClassName().c_str());
            text.append(" was found in the auto-save script but doesn't seem \n"
                        "to exist in the currently loaded plug-ins.");
            throw std::invalid_argument(text.toStdString());
        }
        n->setName(state.getName());
        const NodeGui::SerializedState::KnobValues& knobsValues = state.getKnobsValues();
        //begin changes to params
        for (U32 i = 0; i < knobsValues.size(); ++i) {
            boost::shared_ptr<Knob> knob = n->getKnobByDescription(knobsValues[i].first);
            if(!knob){
                cout << "Couldn't restore knob value ( " << knobsValues[i].first << " )." << endl;
            }else{
                knob->onStartupRestoration(*knobsValues[i].second);
            }
        }
        if(!background){
            NodeGui* nGui = getApp()->getNodeGui(n);
            nGui->setPos(state.getX(),state.getY());
            getApp()->deselectAllNodes();
        }

        n->getLiveInstance()->endValuesChanged(AnimatingParam::PLUGIN_EDITED);
    }
    
    if(!hasProjectAWriter && background){
        clearNodes();
        throw std::invalid_argument("Project file is missing a writer node. This project cannot render anything.");
    }
    
    /*now that we have all nodes, just connect them*/
    for(std::list<NodeGui::SerializedState>::const_iterator it = nodeStates.begin() ; it!=nodeStates.end(); ++it){
        
        if(background && (*it).getClassName() == "Viewer")//ignore viewers on background mode
           continue;
        
        const std::map<int, std::string>& inputs = (*it).getInputs();
        Node* thisNode = NULL;
        for (U32 i = 0; i < _currentNodes.size(); ++i) {
            if (_currentNodes[i]->getName() == (*it).getName()) {
                thisNode = _currentNodes[i];
                break;
            }
        }
        for (std::map<int, std::string>::const_iterator input = inputs.begin(); input!=inputs.end(); ++input) {
            if(input->second.empty())
                continue;
            if(!getApp()->connect(input->first, input->second,thisNode)){
                cout << "Failed to connect " << (*it).getName() << " to " << input->second << endl;
            }
        }
        
    }
    
    QDateTime time = QDateTime::currentDateTime();
    setAutoSetProjectFormat(false);
    setHasProjectBeenSavedByUser(true);
    setProjectName(name);
    setProjectPath(path);
    setProjectAgeSinceLastSave(time);
    setProjectAgeSinceLastAutosaveSave(time);
    
    /*Refresh all viewers as it was*/
    if(!background){
        getApp()->notifyViewersProjectFormatChanged(_availableFormats[_formatKnob->getActiveEntry()]);
        getApp()->checkViewersConnection();
    }
}
void Project::saveProject(const QString& path,const QString& filename,bool autoSave){
    QString filePath;
    if(autoSave){
        filePath = AppInstance::autoSavesDir()+QDir::separator()+filename;
        _lastAutoSaveFilePath = filePath;
    }else{
        filePath = path+filename;
    }
    std::ofstream ofile(filePath.toStdString().c_str(),std::ofstream::out);
    boost::archive::xml_oarchive oArchive(ofile);
    if(!ofile.is_open()){
        cout << "Failed to open file " << filePath.toStdString() << endl;
        return;
    }
    std::list<NodeGui::SerializedState> nodeStates;
    const std::vector<NodeGui*>& activeNodes = getApp()->getAllActiveNodes();
    for (U32 i = 0; i < activeNodes.size(); ++i) {
        NodeGui::SerializedState state = activeNodes[i]->serialize();
        nodeStates.push_back(state);
    }
    oArchive << boost::serialization::make_nvp("Nodes",nodeStates);
    oArchive << boost::serialization::make_nvp("Project_formats",_availableFormats);
    oArchive << boost::serialization::make_nvp("Project_output_format",dynamic_cast<const AnimatingParam&>(*_formatKnob));
    oArchive << boost::serialization::make_nvp("Project_views_count",dynamic_cast<const AnimatingParam&>(*_viewsCount));
    ofile.close();
}



int Project::tryAddProjectFormat(const Format& f){
    for (U32 i = 0; i < _availableFormats.size(); ++i) {
        if(f == _availableFormats[i]){
            return i;
        }
    }
    std::vector<Format> currentFormats = _availableFormats;
    _availableFormats.clear();
    std::vector<std::string> entries;
    for (U32 i = 0; i < currentFormats.size(); ++i) {
        const Format& f = currentFormats[i];
        QString formatStr = generateStringFromFormat(f);
        entries.push_back(formatStr.toStdString());
        _availableFormats.push_back(f);
    }
    QString formatStr = generateStringFromFormat(f);
    entries.push_back(formatStr.toStdString());
    _availableFormats.push_back(f);
    _formatKnob->populate(entries);
    return _availableFormats.size() - 1;
}

void Project::setProjectDefaultFormat(const Format& f) {
    
    int index = tryAddProjectFormat(f);
    _formatKnob->setValue(index);
    getApp()->notifyViewersProjectFormatChanged(f);
    getApp()->triggerAutoSave();
}

 
void Project::createNewFormat(){
    AddFormatDialog dialog(this,getApp()->getGui());
    if(dialog.exec()){
        tryAddProjectFormat(dialog.getFormat());
    }
}

int Project::getProjectViewsCount() const{
    return _viewsCount->getValue<int>();
}
