//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#ifndef NATRON_ENGINE_KNOBTYPES_H_
#define NATRON_ENGINE_KNOBTYPES_H_

#include <vector>
#include <string>
#include <map>

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
#include <QtCore/QObject>
CLANG_DIAG_ON(deprecated)
#include <QVector>
#include <QMutex>

#include "Engine/Knob.h"

#include "Global/GlobalDefines.h"

class Curve;
class ChoiceExtraData;
class OverlaySupport;
class StringAnimationManager;
class BezierCP;
namespace Natron {
class Node;
}
/******************************INT_KNOB**************************************/

class Int_Knob
    : public QObject, public Knob<int>
{
    Q_OBJECT

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Int_Knob(holder, description, dimension,declaredByPlugin);
    }

    Int_Knob(KnobHolder* holder,
             const std::string &description,
             int dimension,
             bool declaredByPlugin);

    void disableSlider();

    bool isSliderDisabled() const;

    static const std::string & typeNameStatic();

public:

    void setIncrement(int incr, int index = 0);

    void setIncrement(const std::vector<int> &incr);

    const std::vector<int> &getIncrements() const;


signals:


    void incrementChanged(int incr, int index = 0);

private:


    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:

    std::vector<int> _increments;
    bool _disableSlider;
    static const std::string _typeNameStr;
};

/******************************BOOL_KNOB**************************************/

class Bool_Knob
    :  public Knob<bool>
{
public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Bool_Knob(holder, description, dimension,declaredByPlugin);
    }

    Bool_Knob(KnobHolder* holder,
              const std::string &description,
              int dimension,
              bool declaredByPlugin);

    /// Can this type be animated?
    /// BooleanParam animation may not be quite perfect yet,
    /// @see Curve::getValueAt() for the animation code.
    static bool canAnimateStatic()
    {
        return true;
    }

    static const std::string & typeNameStatic();

private:

    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    static const std::string _typeNameStr;
};

/******************************DOUBLE_KNOB**************************************/

class Double_Knob
    :  public QObject,public Knob<double>
{
    Q_OBJECT

public:

    enum NormalizedState
    {
        NORMALIZATION_NONE = 0, ///< indicating that the dimension holds a  non-normalized value.
        NORMALIZATION_X, ///< indicating that the dimension holds a value normalized against the X dimension of the project format
        NORMALIZATION_Y ///< indicating that the dimension holds a value normalized against the Y dimension of the project format
    };

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Double_Knob(holder, description, dimension,declaredByPlugin);
    }

    Double_Knob(KnobHolder* holder,
                const std::string &description,
                int dimension,
                bool declaredByPlugin );

    virtual ~Double_Knob();

    void disableSlider();

    bool isSliderDisabled() const;

    const std::vector<double> &getIncrements() const;
    const std::vector<int> &getDecimals() const;

    void setIncrement(double incr, int index = 0);

    void setDecimals(int decis, int index = 0);

    void setIncrement(const std::vector<double> &incr);

    void setDecimals(const std::vector<int> &decis);

    static const std::string & typeNameStatic();

    NormalizedState getNormalizedState(int dimension) const
    {
        assert(dimension < 2 && dimension >= 0);
        if (dimension == 0) {
            return _normalizationXY.first;
        } else {
            return _normalizationXY.second;
        }
    }

    void setNormalizedState(int dimension,
                            NormalizedState state)
    {
        assert(dimension < 2 && dimension >= 0);
        if (dimension == 0) {
            _normalizationXY.first = state;
        } else {
            _normalizationXY.second = state;
        }
    }
    
    void setSpatial(bool spatial);
    bool getIsSpatial() const;

    /**
     * @brief Normalize the default values, set the _defaultStoredNormalized to true and
     * calls setDefaultValue with the good parameters.
     * Later when restoring the default values, this flag will be used to know whether we need
     * to denormalize the default stored values to the set the "live" values.
     * Hence this SHOULD NOT bet set for old deprecated < OpenFX 1.2 normalized parameters otherwise
     * they would be denormalized before being passed to the plug-in.
     *
     * If *all* the following conditions hold:
     * - this is a double value
     * - this is a non normalised spatial double parameter, i.e. kOfxParamPropDoubleType is set to one of
     *   - kOfxParamDoubleTypeX
     *   - kOfxParamDoubleTypeXAbsolute
     *   - kOfxParamDoubleTypeY
     *   - kOfxParamDoubleTypeYAbsolute
     *   - kOfxParamDoubleTypeXY
     *   - kOfxParamDoubleTypeXYAbsolute
     * - kOfxParamPropDefaultCoordinateSystem is set to kOfxParamCoordinatesNormalised
     * Knob<T>::resetToDefaultValue should denormalize
     * the default value, using the input size.
     * Input size be defined as the first available of:
     * - the RoD of the "Source" clip
     * - the RoD of the first non-mask non-optional input clip (in case there is no "Source" clip) (note that if these clips are not connected, you get the current project window, which is the default value for GetRegionOfDefinition)

     * see http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxParamPropDefaultCoordinateSystem
     * and http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#APIChanges_1_2_SpatialParameters
     **/
    void setDefaultValuesNormalized(int dims,double defaults[]);

    /**
     * @brief Same as setDefaultValuesNormalized but for 1 dimensional doubles
     **/
    void setDefaultValuesNormalized(double def)
    {
        double d[1];

        d[0] = def;
        setDefaultValuesNormalized(1,d);
    }

    /**
     * @brief Returns whether the default values are stored normalized or not.
     **/
    bool areDefaultValuesNormalized() const;

    /**
     * @brief Denormalize the given value according to the RoD of the attached effect's input's RoD.
     * WARNING: Can only be called once setNormalizedState has been called!
     **/
    void denormalize(int dimension,double time,double* value) const;

    /**
     * @brief Normalize the given value according to the RoD of the attached effect's input's RoD.
     * WARNING: Can only be called once setNormalizedState has been called!
     **/
    void normalize(int dimension,double time,double* value) const;

    void addSlavedTrack(const boost::shared_ptr<BezierCP> & cp)
    {
        _slavedTracks.push_back(cp);
    }

    void removeSlavedTrack(const boost::shared_ptr<BezierCP> & cp);

    const std::list< boost::shared_ptr<BezierCP> > & getSlavedTracks()
    {
        return _slavedTracks;
    }

    struct SerializedTrack
    {
        std::string rotoNodeName;
        std::string bezierName;
        int cpIndex;
        bool isFeather;
        int offsetTime;
    };

    void serializeTracks(std::list<SerializedTrack>* tracks);

    void restoreTracks(const std::list <SerializedTrack> & tracks,const std::vector<boost::shared_ptr<Natron::Node> > & activeNodes);

public slots:

    void onNodeDeactivated();
    void onNodeActivated();

signals:

    void incrementChanged(double incr, int index = 0);

    void decimalsChanged(int deci, int index = 0);

private:


    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    
    bool _spatial;
    std::vector<double>  _increments;
    std::vector<int> _decimals;
    bool _disableSlider;
    std::list< boost::shared_ptr<BezierCP> > _slavedTracks;

    /// to support ofx deprecated normalizd params:
    /// the first and second dimensions of the double param( hence a pair ) have a normalized state.
    /// BY default they have NORMALIZATION_NONE
    std::pair<NormalizedState, NormalizedState> _normalizationXY;

    ///For double params respecting the kOfxParamCoordinatesNormalised
    ///This tells us that only the default value is stored normalized.
    ///This SHOULD NOT bet set for old deprecated < OpenFX 1.2 normalized parameters.
    bool _defaultStoredNormalized;
    static const std::string _typeNameStr;
};

/******************************BUTTON_KNOB**************************************/

class Button_Knob
    : public Knob<bool>
{
public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Button_Knob(holder, description, dimension,declaredByPlugin);
    }

    Button_Knob(KnobHolder* holder,
                const std::string &description,
                int dimension,
                bool declaredByPlugin);
    static const std::string & typeNameStatic();

    void setAsRenderButton()
    {
        _renderButton = true;
    }

    bool isRenderButton() const
    {
        return _renderButton;
    }

    void setIconFilePath(const std::string & filePath)
    {
        _iconFilePath = filePath;
    }

    const std::string & getIconFilePath() const
    {
        return _iconFilePath;
    }

private:


    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    static const std::string _typeNameStr;
    bool _renderButton;
    std::string _iconFilePath;
};

/******************************CHOICE_KNOB**************************************/

class Choice_Knob
    : public QObject,public Knob<int>
{
    Q_OBJECT

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Choice_Knob(holder, description, dimension,declaredByPlugin);
    }

    Choice_Knob(KnobHolder* holder,
                const std::string &description,
                int dimension,
                bool declaredByPlugin);

    virtual ~Choice_Knob();

    /*Must be called right away after the constructor.*/
    void populateChoices( const std::vector<std::string> &entries, const std::vector<std::string> &entriesHelp = std::vector<std::string>() );
    
    std::vector<std::string> getEntries_mt_safe() const;
    std::vector<std::string> getEntriesHelp_mt_safe() const;
    std::string getActiveEntryText_mt_safe() const;

    /// Can this type be animated?
    /// ChoiceParam animation may not be quite perfect yet,
    /// @see Curve::getValueAt() for the animation code.
    static bool canAnimateStatic()
    {
        return true;
    }

    static const std::string & typeNameStatic();
    std::string getHintToolTipFull() const;
    
    void choiceRestoration(Choice_Knob* knob,const ChoiceExtraData* data);

signals:

    void populated();

private:


    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;
    virtual void deepCloneExtraData(KnobI* other) OVERRIDE FINAL;
private:
    
    mutable QMutex _entriesMutex;
    std::vector<std::string> _entries;
    std::vector<std::string> _entriesHelp;
    static const std::string _typeNameStr;
};

/******************************SEPARATOR_KNOB**************************************/

class Separator_Knob
    : public Knob<bool>
{
public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Separator_Knob(holder, description, dimension,declaredByPlugin);
    }

    Separator_Knob(KnobHolder* holder,
                   const std::string &description,
                   int dimension,
                   bool declaredByPlugin);
    static const std::string & typeNameStatic();

private:


    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    static const std::string _typeNameStr;
};

/******************************RGBA_KNOB**************************************/

/**
 * @brief A color knob with of variable dimension. Each color is a double ranging in [0. , 1.]
 * In dimension 1 the knob will have a single channel being a gray-scale
 * In dimension 3 the knob will have 3 channel R,G,B
 * In dimension 4 the knob will have R,G,B and A channels.
 **/
class Color_Knob
    :  public QObject, public Knob<double>
{
    Q_OBJECT

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Color_Knob(holder, description, dimension,declaredByPlugin);
    }

    Color_Knob(KnobHolder* holder,
               const std::string &description,
               int dimension,
               bool declaredByPlugin);
    
    static const std::string & typeNameStatic();

    bool areAllDimensionsEnabled() const;

    void activateAllDimensions()
    {
        emit mustActivateAllDimensions();
    }

    void setPickingEnabled(bool enabled)
    {
        emit pickingEnabled(enabled);
    }

    /**
     * @brief Convenience function for RGB color params
     **/
    void setValues(double r,double g,double b);


    /**
     * @brief Convenience function for RGBA color params
     **/
    void setValues(double r,double g,double b,double a);

public slots:

    void onDimensionSwitchToggled(bool b);

signals:

    void pickingEnabled(bool);

    void minMaxChanged(double mini, double maxi, int index = 0);

    void displayMinMaxChanged(double mini,double maxi,int index = 0);

    void mustActivateAllDimensions();

private:


    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    bool _allDimensionsEnabled;
    static const std::string _typeNameStr;
};

/******************************STRING_KNOB**************************************/


class String_Knob
    : public AnimatingString_KnobHelper
{
public:


    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new String_Knob(holder, description, dimension,declaredByPlugin);
    }

    String_Knob(KnobHolder* holder,
                const std::string &description,
                int dimension,
                bool declaredByPlugin);

    virtual ~String_Knob();

    /// Can this type be animated?
    /// String animation consists in setting constant strings at
    /// each keyframe, which are valid until the next keyframe.
    /// It can be useful for titling/subtitling.
    static bool canAnimateStatic()
    {
        return true;
    }

    static const std::string & typeNameStatic();

    void setAsMultiLine()
    {
        _multiLine = true;
    }

    void setUsesRichText(bool useRichText)
    {
        _richText = useRichText;
    }

    bool isMultiLine() const
    {
        return _multiLine;
    }

    bool usesRichText() const
    {
        return _richText;
    }

    void setAsLabel()
    {
        setAnimationEnabled(false); //< labels cannot animate
        _isLabel = true;
    }

    bool isLabel() const
    {
        return _isLabel;
    }

    void setAsCustom()
    {
        _isCustom = true;
    }

    bool isCustomKnob() const
    {
        return _isCustom;
    }

private:

    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    static const std::string _typeNameStr;
    bool _multiLine;
    bool _richText;
    bool _isLabel;
    bool _isCustom;
};

/******************************GROUP_KNOB**************************************/
class Group_Knob
    :  public QObject, public Knob<bool>
{
    Q_OBJECT

    std::vector< boost::shared_ptr<KnobI> > _children;
    bool _isTab;

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Group_Knob(holder, description, dimension,declaredByPlugin);
    }

    Group_Knob(KnobHolder* holder,
               const std::string &description,
               int dimension,
               bool declaredByPlugin);

    void addKnob(boost::shared_ptr<KnobI> k);

    const std::vector< boost::shared_ptr<KnobI> > &getChildren() const;

    void setAsTab();

    bool isTab() const;

    static const std::string & typeNameStatic();

private:

    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    static const std::string _typeNameStr;
};


/******************************PAGE_KNOB**************************************/

class Page_Knob
    :  public QObject,public Knob<bool>
{
    Q_OBJECT

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Page_Knob(holder, description, dimension,declaredByPlugin);
    }

    Page_Knob(KnobHolder* holder,
              const std::string &description,
              int dimension,
              bool declaredByPlugin);

    void addKnob(const boost::shared_ptr<KnobI>& k);
    
    const std::vector< boost::shared_ptr<KnobI> > & getChildren() const
    {
        return _children;
    }

    static const std::string & typeNameStatic();

private:
    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:

    std::vector< boost::shared_ptr<KnobI> > _children;
    static const std::string _typeNameStr;
};


/******************************Parametric_Knob**************************************/

class Parametric_Knob
    :  public QObject, public Knob<double>
{
    Q_OBJECT

    mutable QMutex _curvesMutex;
    std::vector< boost::shared_ptr<Curve> > _curves;
    std::vector<RGBAColourF> _curvesColor;

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Parametric_Knob(holder, description, dimension,declaredByPlugin);
    }

    Parametric_Knob(KnobHolder* holder,
                    const std::string &description,
                    int dimension,
                    bool declaredByPlugin );

    void setCurveColor(int dimension,double r,double g,double b);

    void getCurveColor(int dimension,double* r,double* g,double* b);

    void setParametricRange(double min,double max);

    std::pair<double,double> getParametricRange() const WARN_UNUSED_RETURN;
    boost::shared_ptr<Curve> getParametricCurve(int dimension) const;
    Natron::StatusEnum addControlPoint(int dimension,double key,double value) WARN_UNUSED_RETURN;
    Natron::StatusEnum getValue(int dimension,double parametricPosition,double *returnValue) WARN_UNUSED_RETURN;
    Natron::StatusEnum getNControlPoints(int dimension,int *returnValue) WARN_UNUSED_RETURN;
    Natron::StatusEnum getNthControlPoint(int dimension,
                                      int nthCtl,
                                      double *key,
                                      double *value) WARN_UNUSED_RETURN;
    Natron::StatusEnum setNthControlPoint(int dimension,
                                      int nthCtl,
                                      double key,
                                      double value) WARN_UNUSED_RETURN;
    Natron::StatusEnum deleteControlPoint(int dimension, int nthCtl) WARN_UNUSED_RETURN;
    Natron::StatusEnum deleteAllControlPoints(int dimension) WARN_UNUSED_RETURN;
    static const std::string & typeNameStatic() WARN_UNUSED_RETURN;

    void saveParametricCurves(std::list< Curve >* curves) const;

    void loadParametricCurves(const std::list< Curve > & curves);

public slots:

    virtual void drawCustomBackground()
    {
        emit customBackgroundRequested();
    }

    virtual void initializeOverlayInteract(OverlaySupport* widget)
    {
        emit mustInitializeOverlayInteract(widget);
    }

    virtual void resetToDefault(const QVector<int> & dimensions)
    {
        emit mustResetToDefault(dimensions);
    }

signals:

    //emitted by drawCustomBackground()
    //if you can't overload drawCustomBackground()
    void customBackgroundRequested();

    void mustInitializeOverlayInteract(OverlaySupport*);

    ///emitted when the state of a curve changed at the indicated dimension
    void curveChanged(int);

    void mustResetToDefault(QVector<int>);

private:

    virtual void resetExtraToDefaultValue(int dimension) OVERRIDE FINAL;

    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;
    virtual void cloneExtraData(KnobI* other,int dimension = -1) OVERRIDE FINAL;
    virtual void cloneExtraData(KnobI* other, SequenceTime offset, const RangeD* range,int dimension = -1) OVERRIDE FINAL;
    static const std::string _typeNameStr;
};

#endif // NATRON_ENGINE_KNOBTYPES_H_
