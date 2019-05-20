__author__ = "Vahan Sosoyan"
__copyright__ = "2019 All rights reserved. See Copyright.txt for more details."
__version__ = "1.3.5"

import sys
from timeit import default_timer

import maya.mel as mel
import maya.OpenMaya as OM
import pymel.core as pm
from maya import cmds, OpenMayaUI
from maya.app.general.mayaMixin import MayaQWidgetDockableMixin, MayaQDockWidget

try:
    from arnold import *
    import mtoa.core as core
except ImportError:
    cmds.warning("MtoA was not found.")

# Check Maya and Arnold versions
MAYA_2017 = True if (cmds.about(api=True) >= 201700) else False
ARNOLD_5 = True if AiGetVersion()[0] == '5' else False

if MAYA_2017:
    from PySide2 import QtCore, QtWidgets
    from shiboken2 import wrapInstance
else:
    from PySide import QtCore
    from PySide import QtGui as QtWidgets
    from shiboken import wrapInstance

class BoxWidget(QtWidgets.QFrame):
    def __init__(self, label, first = True):
        super(BoxWidget, self).__init__()
        self.label = QtWidgets.QLabel(label + ":")
        self.label.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignCenter)

        if first:
            self.label.setMinimumSize(75, 20)
            self.label.setMaximumSize(75, 20)

        self.layout = QtWidgets.QHBoxLayout(self)
        self.layout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
        self.layout.setContentsMargins(0, 0, 0, 0)
        self.layout.addWidget(self.label)

class LineEditBox(BoxWidget):
    def __init__(self, label, text = '', first=True):
        super(LineEditBox, self).__init__(label, first)

        self.lineEditBox = QtWidgets.QLineEdit()
        self.lineEditBox.setText(u"%s"%text)
        self.layout.addWidget(self.lineEditBox)

    def setEnabled(self, value):
        self.label.setEnabled(value)
        self.lineEditBox.setEnabled(value)

    def text(self):
        return self.lineEditBox.text()

    def setText(self, text):
        self.lineEditBox.setText(text)

class SliderBox(BoxWidget):
    def __init__(self, label, value = 0, first=True):
        super(SliderBox, self).__init__(label, first)

        self.spinBox = QtWidgets.QSpinBox()
        self.spinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.spinBox.setValue(value)

        self.slider = QtWidgets.QSlider()
        self.slider.setOrientation(QtCore.Qt.Horizontal)
        self.slider.setValue(value)

        self.slider.valueChanged.connect(self.spinBox.setValue)

        self.layout.addWidget(self.spinBox)
        self.layout.addWidget(self.slider)

    def setMinimum(self, spinValue=None, sliderValue=None):
        if spinValue is not None: self.spinBox.setMinimum(spinValue)
        if sliderValue is not None: self.slider.setMinimum(sliderValue)

    def setMaximum(self, spinValue=None, sliderValue=None):
        if spinValue is not None: self.spinBox.setMaximum(spinValue)
        if sliderValue is not None: self.slider.setMaximum(sliderValue)

    def setValue(self, spinValue=None, sliderValue=None):
        if sliderValue is not None: self.slider.setValue(sliderValue)
        if spinValue is not None: self.spinBox.setValue(spinValue)

    def value(self):
        return self.spinBox.value()

    def connect(self, func):
        self.slider.valueChanged.connect(func)

    def setEnabled(self, value):
        self.label.setEnabled(value)
        self.spinBox.setEnabled(value)
        self.slider.setEnabled(value)

    @property
    def valueChanged(self):
        return self.spinBox.valueChanged

class SpinBox(BoxWidget):
    def __init__(self, label, value=0, first=True):
        super(SpinBox, self).__init__(label, first)
        self.spinBox = QtWidgets.QSpinBox()
        self.spinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.spinBox.setValue(value)
        self.spinBox.setMaximumSize(50, 20)
        self.spinBox.setRange(-99999,99999)

        self.layout.addWidget(self.spinBox)

    def value(self):
        return self.spinBox.value()

    def setValue(self, value):
        self.spinBox.setValue(value)

    @property
    def valueChanged(self):
        return self.spinBox.valueChanged

class ComboBox(BoxWidget):
    def __init__(self, label, first=True):
        super(ComboBox, self).__init__(label, first)
        self.items = list()

        self.comboBox = QtWidgets.QComboBox()
        self.comboBox.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        self.currentIndexChanged = self.comboBox.currentIndexChanged

        self.layout.addWidget(self.comboBox)

    def setEnabled(self, value):
        self.label.setEnabled(value)
        self.comboBox.setEnabled(value)

    def setCurrentIndex(self, value):
        self.comboBox.setCurrentIndex(value)

    def currentIndex(self):
        return self.comboBox.currentIndex()

    def currentName(self):
        index = self.comboBox.currentIndex()
        return self.items[index]

    def addItems(self, items):
        for i in items:
            self.comboBox.addItem(i)
        self.items += items

def getMayaMainWindow():
    main_window_ptr = OpenMayaUI.MQtUtil.mainWindow()
    return wrapInstance(long(main_window_ptr), QtWidgets.QMainWindow)

def getHost():
    ''' Returns the host from Aton driver '''
    host = 0
    try: # To init Arnold Render settings
        host = cmds.getAttr("defaultArnoldDisplayDriver.host")
    except ValueError:
        mel.eval("unifiedRenderGlobalsWindow;")
        try: # If aton driver is not loaded
            host = cmds.getAttr("defaultArnoldDisplayDriver.host")
        except ValueError:
            pass
    return host

def getPort():
    ''' Returns the port number from Aton driver '''
    port = 0
    try: # To init Arnold Render settings
        port = cmds.getAttr("defaultArnoldDisplayDriver.port")
    except ValueError:
        mel.eval("unifiedRenderGlobalsWindow;")
        try: # If aton driver is not loaded
            port = cmds.getAttr("defaultArnoldDisplayDriver.port")
        except ValueError:
            pass
    return port

def getActiveCamera():
    ''' Returns active camera shape name '''
    cam = cmds.modelEditor(cmds.playblast(ae=1), q=1, cam=1)
    if cmds.listRelatives(cam) != None:
        cam = cmds.listRelatives(cam)[0]
    return cam

def getSceneOption(attr):
    ''' Returns requested scene options attribute value '''
    if cmds.getAttr("defaultRenderGlobals.ren") == "arnold":
        try:
            return {0 : lambda: getHost(),
                    1 : lambda: getPort(),
                    2 : lambda: getActiveCamera(),
                    3 : lambda: cmds.getAttr("defaultResolution.width"),
                    4 : lambda: cmds.getAttr("defaultResolution.height"),
                    5 : lambda: cmds.getAttr("defaultArnoldRenderOptions.AASamples"),
                    6 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreMotionBlur"),
                    7 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreSubdivision"),
                    8 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreDisplacement"),
                    9 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreBump"),
                    10 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreSss"),
                    11 : lambda: cmds.playbackOptions(q=True, minTime=True),
                    12 : lambda: cmds.playbackOptions(q=True, maxTime=True),
                    13 : lambda: cmds.getAttr("defaultArnoldRenderOptions.progressive_rendering"),
                    14 : lambda: cmds.getAttr("defaultArnoldRenderOptions.bucketScanning")}[attr]()
        except ValueError:
            pass
    return 0

class Aton(MayaQWidgetDockableMixin, QtWidgets.QWidget):
    def __init__(self):
        # Attributes
        self.timeChangedCB = None
        self.selectionChangedCB = None
        self.defaultMergeAOVs = None
        self.defaultAiTranslator = None
        self.defaultHost = getSceneOption(0)
        self.defaultPort = getSceneOption(1)

        # Sequence mode
        self.frame_sequence = AiFrameSequence()
        self.frame_sequence.started.connect(self.sequence_started)
        self.frame_sequence.stopped.connect(self.sequence_stopped)
        self.frame_sequence.stepped.connect(self.sequence_stepped)

        # UI Names
        self.objName = self.__class__.__name__.lower()
        self.wsCtrlName = self.objName + 'WorkspaceControl'

        # Delete already existing UI instances
        self.deleteInstances()

        # Init UI
        super(Aton, self).__init__(getMayaMainWindow())
        self.setObjectName(self.objName)
        self.setWindowTitle(self.__class__.__name__)
        self.setProperty("saveWindowPref", True)
        self.setupUI()

    def setupUI(self):
        ''' Building the GUI '''
        def resUpdateUI(value):
            self.resolutionSlider.setValue(value * 5)
            xres = getSceneOption(3) * value * 5 / 100
            yres = getSceneOption(4) * value * 5 / 100
            resolutionInfoLabel.setText("%sx%s"%(xres, yres))

        def portUpdateUI(value):
            self.portSlider.spinBox.setValue(value + self.defaultPort)

        def sequence_toggled(value):
            self.startSpinBox.setEnabled(value)
            self.endSpinBox.setEnabled(value)
            self.stepSpinBox.setEnabled(value)

        def resetUI(*args):
            self.hostLineEdit.setText(self.defaultHost)
            self.hostCheckBox.setChecked(True)
            self.portSlider.setValue(self.defaultPort, 0)
            self.portCheckBox.setChecked(True)
            self.cameraComboBox.setCurrentIndex(0)
            self.resolutionSlider.setValue(100, 20)
            self.cameraAaSlider.setValue(getSceneOption(5))
            self.renderRegionXSpinBox.setValue(0)
            self.renderRegionYSpinBox.setValue(0)
            self.renderRegionRSpinBox.setValue(getSceneOption(3))
            self.renderRegionTSpinBox.setValue(getSceneOption(4))
            self.overscanSlider.setValue(0, 0)
            self.motionBlurCheckBox.setChecked(getSceneOption(6))
            self.subdivsCheckBox.setChecked(getSceneOption(7))
            self.displaceCheckBox.setChecked(getSceneOption(8))
            self.bumpCheckBox.setChecked(getSceneOption(9))
            self.sssCheckBox.setChecked(getSceneOption(10))
            self.shaderComboBox.setCurrentIndex(0)
            self.textureRepeatSlider.setValue(1, 1)
            self.selectedShaderCheckbox.setChecked(0)
            self.startSpinBox.setValue(getSceneOption(11))
            self.endSpinBox.setValue(getSceneOption(12))
            self.stepSpinBox.setValue(1)
            self.seqCheckBox.setChecked(False)

        self.setAttribute(QtCore.Qt.WA_AlwaysShowToolTips)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)

        mainLayout = QtWidgets.QVBoxLayout()

        # GENERAL GROUP
        generalGroupBox = QtWidgets.QGroupBox("General")
        generalGroupBox.setMaximumSize(9999, 150)
        generalLayout = QtWidgets.QVBoxLayout(generalGroupBox)

        # Host Layout
        hostLayout = QtWidgets.QHBoxLayout()
        self.hostLineEdit = LineEditBox("Host", u"%s"%self.defaultHost)
        self.hostCheckBox = QtWidgets.QCheckBox()
        self.hostCheckBox.setChecked(True)
        self.hostCheckBox.stateChanged.connect(self.hostLineEdit.setEnabled)
        hostLayout.addWidget(self.hostLineEdit)
        hostLayout.addWidget(self.hostCheckBox)

        # Port Layout
        portLayout = QtWidgets.QHBoxLayout()
        self.portSlider = SliderBox("Port")
        self.portSlider.setMinimum(0, 0)
        self.portSlider.setMaximum(9999, 15)
        self.portSlider.setValue(self.defaultPort)
        self.portSlider.connect(portUpdateUI)
        self.portCheckBox = QtWidgets.QCheckBox()
        self.portCheckBox.setChecked(True)
        self.portCheckBox.stateChanged.connect(self.portSlider.setEnabled)
        portLayout.addWidget(self.portSlider)
        portLayout.addWidget(self.portCheckBox)

        # Camera Layout
        cameraLayout = QtWidgets.QHBoxLayout()
        self.cameraComboBox = ComboBox("Camera")
        self.cameraComboBox.addItems(["Current view"] + cmds.listCameras())
        cameraLayout.addWidget(self.cameraComboBox)

        # OVERRIDES GROUP
        overridesGroupBox = QtWidgets.QGroupBox("Overrides")
        overridesGroupBox.setMaximumSize(9999, 450)
        overridesLayout = QtWidgets.QVBoxLayout(overridesGroupBox)

        # Bucket Layout
        bucketLayout = QtWidgets.QHBoxLayout()
        self.bucketComboBox = ComboBox("Bucket Scan")
        self.bucketComboBox.addItems(['top', 'left', 'random', 'spiral', 'hilbert'])
        self.bucketComboBox.setCurrentIndex(getSceneOption(14))
        bucketLayout.addWidget(self.bucketComboBox)

        # Resolution Layout
        resolutionLayout = QtWidgets.QHBoxLayout()
        self.resolutionSlider = SliderBox("Resolution %")
        self.resolutionSlider.setMinimum(1, 1)
        self.resolutionSlider.setMaximum(200, 40)
        self.resolutionSlider.setValue(100, 20)
        self.resolutionSlider.connect(resUpdateUI)
        xres, yres = getSceneOption(3), getSceneOption(4)
        resolutionInfoLabel = QtWidgets.QLabel(str(xres)+'x'+str(yres))
        resolutionInfoLabel.setMaximumSize(100, 20)
        resolutionInfoLabel.setEnabled(False)
        resolutionLayout.addWidget(self.resolutionSlider)
        resolutionLayout.addWidget(resolutionInfoLabel)

        # Camera AA Layout
        cameraAaLayout = QtWidgets.QHBoxLayout()
        self.cameraAaSlider = SliderBox("Camera (AA)")
        self.cameraAaSlider.setMinimum(-64, -3)
        self.cameraAaSlider.setMaximum(64, 16)
        self.cameraAaSlider.setValue(getSceneOption(5))
        cameraAaLayout.addWidget(self.cameraAaSlider)

        # Render region layout
        renderRegionLayout = QtWidgets.QHBoxLayout()
        self.renderRegionXSpinBox = SpinBox("Region X")
        self.renderRegionYSpinBox = SpinBox("Y", 0, False)
        self.renderRegionRSpinBox = SpinBox("R", 0, False)
        self.renderRegionTSpinBox = SpinBox("T", 0, False)
        self.renderRegionRSpinBox.setValue(getSceneOption(3))
        self.renderRegionTSpinBox.setValue(getSceneOption(4))
        renderRegionGetNukeButton = QtWidgets.QPushButton("Get")
        renderRegionGetNukeButton.clicked.connect(self.getNukeCropNode)
        renderRegionLayout.addWidget(self.renderRegionXSpinBox)
        renderRegionLayout.addWidget(self.renderRegionYSpinBox)
        renderRegionLayout.addWidget(self.renderRegionRSpinBox)
        renderRegionLayout.addWidget(self.renderRegionTSpinBox)
        renderRegionLayout.addWidget(renderRegionGetNukeButton)

        # Overscan Layout
        overscanLayout = QtWidgets.QHBoxLayout()
        self.overscanSlider = SliderBox("Overscan")
        self.overscanSlider.setMinimum(0)
        self.overscanSlider.setMaximum(9999, 250)
        self.overscanSlider.setValue(0, 0)
        overscanSetButton = QtWidgets.QPushButton("Set")
        overscanLayout.addWidget(self.overscanSlider)
        overscanLayout.addWidget(overscanSetButton)

        # Shaders layout
        shaderLayout = QtWidgets.QHBoxLayout()
        self.shaderComboBox = ComboBox("Shader override", False)
        self.shaderComboBox.addItems(["Disabled", "Checker", "Grey", "Mirror", "Normal", "Occlusion", "UV"])
        self.selectedShaderCheckbox = QtWidgets.QCheckBox("Selected objects only")
        shaderLayout.addWidget(self.shaderComboBox)
        shaderLayout.addWidget(self.selectedShaderCheckbox)

        textureRepeatLayout = QtWidgets.QHBoxLayout()
        self.textureRepeatSlider = SliderBox("Texture repeat", 1, False)
        self.textureRepeatSlider.setMinimum(None, 1)
        self.textureRepeatSlider.setMaximum(None, 64)
        textureRepeatLayout.addWidget(self.textureRepeatSlider)

        # IGNORE GROUP
        ignoresGroupBox = QtWidgets.QGroupBox("Ignore")
        ignoresGroupBox.setMaximumSize(9999, 75)

        # Ignore Layout
        ignoresLayout = QtWidgets.QVBoxLayout(ignoresGroupBox)
        ignoreLayout = QtWidgets.QHBoxLayout()
        ignoreLabel = QtWidgets.QLabel("Ignore:")
        ignoreLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        self.motionBlurCheckBox = QtWidgets.QCheckBox("Motion Blur")
        self.motionBlurCheckBox.setChecked(getSceneOption(6))
        self.subdivsCheckBox = QtWidgets.QCheckBox("Subdivs")
        self.subdivsCheckBox.setChecked(getSceneOption(7))
        self.displaceCheckBox = QtWidgets.QCheckBox("Displace")
        self.displaceCheckBox.setChecked(getSceneOption(8))
        self.bumpCheckBox = QtWidgets.QCheckBox("Bump")
        self.bumpCheckBox.setChecked(getSceneOption(9))
        self.sssCheckBox = QtWidgets.QCheckBox("SSS")
        self.sssCheckBox.setChecked(getSceneOption(10))
        ignoreLayout.addWidget(self.motionBlurCheckBox)
        ignoreLayout.addWidget(self.subdivsCheckBox)
        ignoreLayout.addWidget(self.displaceCheckBox)
        ignoreLayout.addWidget(self.bumpCheckBox)
        ignoreLayout.addWidget(self.sssCheckBox)

        # SEQUENCE GROUP
        sequenceGroupBox = QtWidgets.QGroupBox('Sequence')
        sequenceGroupBox.setMaximumSize(9999, 75)

        # Sequence Layout
        sequenceLayout = QtWidgets.QHBoxLayout(sequenceGroupBox)
        self.seqCheckBox = QtWidgets.QCheckBox()
        self.seqCheckBox.setMaximumSize(15, 25)
        self.seqCheckBox.stateChanged.connect(sequence_toggled)
        self.startSpinBox = SpinBox('Start frame', False)
        self.startSpinBox.setValue(getSceneOption(11))
        self.startSpinBox.setEnabled(False)
        self.endSpinBox = SpinBox('End frame', False)
        self.endSpinBox.setValue(getSceneOption(12))
        self.endSpinBox.setEnabled(False)
        self.stepSpinBox = SpinBox('By frame', False)
        self.stepSpinBox.setValue(1)
        self.stepSpinBox.setEnabled(False)
        sequenceLayout.addWidget(self.seqCheckBox)
        sequenceLayout.addWidget(self.startSpinBox)
        sequenceLayout.addWidget(self.endSpinBox)
        sequenceLayout.addWidget(self.stepSpinBox)

        # Main Buttons Layout
        mainButtonslayout = QtWidgets.QHBoxLayout()
        startButton = QtWidgets.QPushButton("Start / Refresh")
        stopButton = QtWidgets.QPushButton("Stop")
        resetButton = QtWidgets.QPushButton("Reset")
        startButton.clicked.connect(self.render)
        stopButton.clicked.connect(self.stop)
        resetButton.clicked.connect(resetUI)
        mainButtonslayout.addWidget(startButton)
        mainButtonslayout.addWidget(stopButton)
        mainButtonslayout.addWidget(resetButton)

        # Add Layouts to Main
        generalLayout.addLayout(hostLayout)
        generalLayout.addLayout(portLayout)
        generalLayout.addLayout(cameraLayout)
        overridesLayout.addLayout(bucketLayout)
        overridesLayout.addLayout(resolutionLayout)
        overridesLayout.addLayout(cameraAaLayout)
        overridesLayout.addLayout(renderRegionLayout)
        overridesLayout.addLayout(overscanLayout)
        overridesLayout.addLayout(shaderLayout)
        overridesLayout.addLayout(textureRepeatLayout)
        ignoresLayout.addLayout(ignoreLayout)

        mainLayout.addWidget(generalGroupBox)
        mainLayout.addWidget(overridesGroupBox)
        mainLayout.addWidget(ignoresGroupBox)
        mainLayout.addWidget(sequenceGroupBox)
        mainLayout.addLayout(mainButtonslayout)

        # IPR Updates
        self.cameraComboBox.currentIndexChanged.connect(lambda: self.IPRUpdate(0))
        self.bucketComboBox.currentIndexChanged.connect(lambda: self.IPRUpdate(6))
        self.resolutionSlider.valueChanged.connect(lambda: self.IPRUpdate(1))
        self.cameraAaSlider.valueChanged.connect(lambda: self.IPRUpdate(2))
        self.renderRegionXSpinBox.valueChanged.connect(lambda: self.IPRUpdate(1))
        self.renderRegionYSpinBox.valueChanged.connect(lambda: self.IPRUpdate(1))
        self.renderRegionRSpinBox.valueChanged.connect(lambda: self.IPRUpdate(1))
        self.renderRegionTSpinBox.valueChanged.connect(lambda: self.IPRUpdate(1))
        self.overscanSlider.valueChanged.connect(lambda: self.IPRUpdate(1))
        self.motionBlurCheckBox.toggled.connect(lambda: self.IPRUpdate(3))
        self.subdivsCheckBox.toggled.connect(lambda: self.IPRUpdate(3))
        self.displaceCheckBox.toggled.connect(lambda: self.IPRUpdate(3))
        self.bumpCheckBox.toggled.connect(lambda: self.IPRUpdate(3))
        self.sssCheckBox.toggled.connect(lambda: self.IPRUpdate(3))
        self.shaderComboBox.currentIndexChanged.connect(lambda: self.IPRUpdate(4))
        self.textureRepeatSlider.valueChanged.connect(lambda: self.IPRUpdate(5))
        self.selectedShaderCheckbox.toggled.connect(lambda: self.IPRUpdate(4))

        self.setLayout(mainLayout)

    def deleteInstances(self):
        ''' Delete any instances of this class '''
        mayaWindow = getMayaMainWindow()
        if MAYA_2017:
            if cmds.workspaceControl(self.wsCtrlName, q=True, exists=True):
                cmds.deleteUI(self.wsCtrlName, control=True)
        else:
            for obj in mayaWindow.children():
                if type(obj) == MayaQDockWidget:
                    if obj.widget().objectName() == self.objName:
                        mayaWindow.removeDockWidget(obj)
                        obj.setParent(None)
                        obj.deleteLater()

    def show(self, docked=True):
        if docked:
            super(self.__class__, self).show(dockable=True, area='right', floating=False)
            if MAYA_2017:
                cmds.workspaceControl(self.wsCtrlName, e=True, ttc=["AttributeEditor", -1])
            self.raise_()
        else:
            super(self.__class__, self).show(dockable=True)

    def getCamera(self):
        ''' Returns current selected camera from GUI '''
        if self.cameraComboBox.currentIndex() == 0:
            camera = getSceneOption(2)
        else:
            camera = self.cameraComboBox.currentName()
            if cmds.listRelatives(camera, s=1) != None:
                camera = cmds.listRelatives(camera, s=1)[0]
        return camera

    def getRegion(self, attr, resScale = True):
        if resScale:
            resValue = self.resolutionSlider.value()
        else:
            resValue = 100

        ovrScnValue = self.overscanSlider.value() * resValue / 100

        xres = getSceneOption(3) * resValue / 100
        yres = getSceneOption(4) * resValue / 100

        result = {0 : lambda: xres,
                  1 : lambda: yres,
                  2 : lambda: (self.renderRegionXSpinBox.value() * resValue / 100) - ovrScnValue,
                  3 : lambda: yres - (self.renderRegionTSpinBox.value() * resValue / 100) - ovrScnValue,
                  4 : lambda: (self.renderRegionRSpinBox.value() * resValue / 100) - 1 + ovrScnValue,
                  5 : lambda: (yres - (self.renderRegionYSpinBox.value() * resValue / 100)) - 1 + ovrScnValue}[attr]()

        return result

    def getNukeCropNode(self, *args):
        ''' Get crop node data from Nuke '''
        def find_between(s, first, last):
            try:
                start = s.index(first) + len(first)
                end = s.index(last, start)
                return s[start:end]
            except ValueError:
                return ""

        clipboard = QtWidgets.QApplication.clipboard()
        data = clipboard.text()

        cropData = None
        checkData1 = "set cut_paste_input [stack 0]"
        checkData2 = "Crop {"

        if (checkData1 in data.split('\n', 10)[0]) and \
           (checkData2 in data.split('\n', 10)[3]):
                cropData = find_between(data.split('\n', 10)[4], "box {", "}" ).split()

        if len(data.split(',')) == 4:
            cropData = data.split(',')

        if cropData is not None:
            nkX, nkY, nkR, nkT = int(float(cropData[0])),\
                                 int(float(cropData[1])),\
                                 int(float(cropData[2])),\
                                 int(float(cropData[3]))

            self.renderRegionXSpinBox.setValue(nkX)
            self.renderRegionYSpinBox.setValue(nkY)
            self.renderRegionRSpinBox.setValue(nkR)
            self.renderRegionTSpinBox.setValue(nkT)


    def setOverscan(self):
        ovrScnValue = bool(self.overscanSlider.value())
        if cmds.getAttr("defaultRenderGlobals.ren") == "arnold":
            message = "Do you want to set the Overscan values in Render Setttings?"
            result = cmds.confirmDialog(title='Overscan',
                                        message=message,
                                        button=['OK', 'Cancel'],
                                        defaultButton='OK',
                                        cancelButton='Cancel',
                                        dismissString='Cancel',
                                        icn="information")
            if result == 'OK':
                rMinX = self.getRegion(2, False)
                rMinY = self.getRegion(3, False)
                rMaxX = self.getRegion(4, False)
                rMaxY = self.getRegion(5, False)
                attr = "defaultArnoldRenderOptions.outputOverscan"
                if ovrScnValue:
                    cmds.setAttr(attr, "%s %s %s %s"%(rMinX, rMinY, rMaxX, rMaxY), type="string")
                else:
                    cmds.setAttr(attr, "", type="string")

    def render(self):
        ''' Starts the render '''
        try: # If MtoA was not found
            self.defaultMergeAOVs = cmds.getAttr("defaultArnoldDriver.mergeAOVs")
            self.defaultAiTranslator = cmds.getAttr("defaultArnoldDisplayDriver.aiTranslator")
        except ValueError:
            cmds.warning("Current renderer is not set to Arnold.")
            return

        # Setting necessary options
        cmds.setAttr("defaultArnoldDriver.mergeAOVs",  True)
        cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", "aton", type="string")

        # Updating host and port from UI
        if self.defaultPort != 0:
            if self.hostCheckBox.isChecked():
                host = self.hostLineEdit.text()
                cmds.setAttr("defaultArnoldDisplayDriver.host", host, type="string")
            if self.portCheckBox.isChecked():
                port = self.portSlider.value()
                cmds.setAttr("defaultArnoldDisplayDriver.port", port)
        else:
            cmds.warning("Current renderer is not set to Arnold or Aton driver is not loaded.")
            return

        # Set RenderLayer
        currentRenderLayer = cmds.editRenderLayerGlobals(q=1, crl=1)
        cmds.setAttr("defaultArnoldDisplayDriver.output", currentRenderLayer, type="string")

        # Adding time changed callback
        if self.timeChangedCB == None:
            self.timeChangedCB = OM.MEventMessage.addEventCallback("timeChanged", self.timeChnaged)

        # Adding selection changed callback
        if self.selectionChangedCB == None:
            self.selectionChangedCB = OM.MEventMessage.addEventCallback('SelectionChanged', self.selectionChanged)

        try: # If render session is not started yet
            cmds.arnoldIpr(mode='stop')
        except RuntimeError:
            pass

        # Temporary makeing hidden cameras visible before scene export
        hCams = []
        for i in cmds.listCameras():
            if not cmds.getAttr(i + ".visibility"):
                hCams.append(i)
            sl = cmds.listRelatives(i, s=1)
            if sl and not cmds.getAttr(sl[0] + ".visibility"):
                hCams.append(sl[0])

        for i in hCams: cmds.showHidden(i)

        # Set Progressive refinement to off
        if self.sequence_enabled:
            self.defaultRefinement = getSceneOption(13)
            cmds.setAttr("defaultArnoldRenderOptions.progressive_rendering", False)

        try: # Start IPR
            cmds.arnoldIpr(cam=self.getCamera(), mode='start')
            sys.stdout.write("// Info: Aton - Render started.\n")
        except RuntimeError:
            cmds.warning("Current renderer is not set to Arnold.")

        # Sequence Rendering
        if self.sequence_enabled:
            self.frame_sequence.start()

        # Update IPR
        self.IPRUpdate()

        # Setting back to default
        for i in hCams:
            cmds.hide(i)
        cmds.setAttr("defaultArnoldDriver.mergeAOVs",  self.defaultMergeAOVs)
        cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", self.defaultAiTranslator, type="string")

        if self.hostCheckBox.isChecked():
            cmds.setAttr("defaultArnoldDisplayDriver.host", host, type="string")
        if self.portCheckBox.isChecked():
            cmds.setAttr("defaultArnoldDisplayDriver.port", self.defaultPort)

    def getFrames(self):
        frames = range(self.startSpinBox.value(),
                       self.endSpinBox.value() + 1,
                       self.stepSpinBox.value())
        return frames

    @property
    def sequence_enabled(self):
        return self.seqCheckBox.checkState()

    def sequence_started(self):
        # Setup frame_sequence
        self.frame_sequence.frames = self.getFrames()

        # Setup progress bar
        progressKeys = {"edit":True,
                        "beginProgress":True,
                        "isInterruptable":True,
                        "maxValue": len(self.frame_sequence.frames),
                        "status": 'Aton Frame Sequence'}

        self.gMainProgressBar = mel.eval('$tmp = $gMainProgressBar')
        cmds.progressBar( self.gMainProgressBar, **progressKeys)
        # maya.api bugfix
        if cmds.progressBar(self.gMainProgressBar, q=True, ic=True):
            cmds.progressBar(self.gMainProgressBar, e=True, ep=True)
            cmds.progressBar(self.gMainProgressBar, **progressKeys)

    def sequence_stopped(self):
        # Stop ipr when finished
        self.stop()

        # Restore default progressive refinement
        cmds.setAttr("defaultArnoldRenderOptions.progressive_rendering", self.defaultRefinement)

        # kill progressBar
        cmds.progressBar(self.gMainProgressBar, edit=True, endProgress=True)

    def sequence_stepped(self, frame):
        # Refresh IPR
        self.IPRUpdate()

        # step progressBar
        cmds.progressBar(self.gMainProgressBar, edit=True, step=1)

    def initOvrShaders(self):
        ''' Initilize override shaders '''
        # Checker shader
        self.checkerShader = AiNode("standard")
        checkerTexture = AiNode("MayaChecker")
        self.placeTexture = AiNode("MayaPlace2DTexture")
        AiNodeLink(self.placeTexture, "uvCoord", checkerTexture)
        AiNodeLink(checkerTexture, "Kd", self.checkerShader)

        # Grey Shader
        self.greyShader = AiNode("standard")
        AiNodeSetFlt(self.greyShader, "Kd", 0.225)
        AiNodeSetFlt(self.greyShader, "Ks", 1)
        AiNodeSetFlt(self.greyShader, "specular_roughness", 0.3)
        AiNodeSetBool(self.greyShader, "specular_Fresnel", True)
        AiNodeSetBool(self.greyShader, "Fresnel_use_IOR", True)
        AiNodeSetFlt(self.greyShader, "IOR", 1.3)

        # Mirror Shader
        self.mirrorShader = AiNode("standard")
        AiNodeSetFlt(self.mirrorShader, "Kd", 0)
        AiNodeSetFlt(self.mirrorShader, "Ks", 1)
        AiNodeSetFlt(self.mirrorShader, "specular_roughness", 0.005)
        AiNodeSetBool(self.mirrorShader, "specular_Fresnel", True)
        AiNodeSetFlt(self.mirrorShader, "Ksn", 0.6)

        # Normal Shader
        self.normalShader = AiNode("utility")
        AiNodeSetInt(self.normalShader, "shade_mode", 2)
        AiNodeSetInt(self.normalShader, "color_mode", 2)

        # Occlusion Shader
        self.occlusionShader = AiNode("utility")
        AiNodeSetInt(self.occlusionShader, "shade_mode", 3)

        # UV Shader
        self.uvShader = AiNode("utility")
        AiNodeSetInt(self.uvShader, "shade_mode", 2)
        AiNodeSetInt(self.uvShader, "color_mode", 5)

    def IPRUpdate(self, attr=None):
        ''' This method is called during IPR session '''
        try: # If render session is not started yet
            cmds.arnoldIpr(mode='pause')
        except (AttributeError, RuntimeError):
            return

        options = AiUniverseGetOptions()

        # Camera Update
        if attr == None or attr == 0:
            camera = self.getCamera()
            iterator = AiUniverseGetNodeIterator(AI_NODE_CAMERA)
            while not AiNodeIteratorFinished(iterator):
                node = AiNodeIteratorGetNext(iterator)
                if AiNodeGetName(node) == camera:
                    AiNodeSetPtr(options, "camera", node)

        # Resolution and Region Update
        if attr == None or attr == 1:

            AiNodeSetInt(options, "xres", self.getRegion(0))
            AiNodeSetInt(options, "yres", self.getRegion(1))

            AiNodeSetInt(options, "region_min_x", self.getRegion(2))
            AiNodeSetInt(options, "region_min_y", self.getRegion(3))
            AiNodeSetInt(options, "region_max_x", self.getRegion(4))
            AiNodeSetInt(options, "region_max_y", self.getRegion(5))

        # Camera AA Update
        if attr == None or attr == 2:
            cameraAA = self.cameraAaSlider.value()
            options = AiUniverseGetOptions()
            AiNodeSetInt(options, "AA_samples", cameraAA)

        # Ignore options Update
        if attr == None or attr == 3:
            motionBlur = self.motionBlurCheckBox.isChecked()
            subdivs = self.subdivsCheckBox.isChecked()
            displace = self.displaceCheckBox.isChecked()
            bump = self.bumpCheckBox.isChecked()
            sss = self.sssCheckBox.isChecked()

            AiNodeSetBool(options, "ignore_motion_blur", motionBlur)
            AiNodeSetBool(options, "ignore_subdivision", subdivs)
            AiNodeSetBool(options, "ignore_displacement", displace)
            AiNodeSetBool(options, "ignore_bump", bump)
            AiNodeSetBool(options, "ignore_sss", sss)

        # Storing default shader assignments
        if attr == None:
            self.initOvrShaders()
            self.shadersDict = {}
            iterator = AiUniverseGetNodeIterator(AI_NODE_SHAPE)
            while not AiNodeIteratorFinished(iterator):
                node = AiNodeIteratorGetNext(iterator)
                name = AiNodeGetName(node)
                try: # If object name is not exist i.e. "root"
                    sgList = cmds.listConnections(name, type='shadingEngine')
                    if sgList > 0:
                        self.shadersDict[name] = AiNodeGetPtr(node, "shader")
                except ValueError:
                    continue

        # Shader override Update
        shaderIndex = self.shaderComboBox.currentIndex()
        if attr == 4 or shaderIndex > 0:
            iterator = AiUniverseGetNodeIterator(AI_NODE_SHAPE)
            while not AiNodeIteratorFinished(iterator):
                node = AiNodeIteratorGetNext(iterator)
                name = AiNodeGetName(node)

                selChecked = self.selectedShaderCheckbox.isChecked()
                if shaderIndex != 0 and selChecked:
                    selectionList = cmds.ls(dag=1, sl=1, s=1)
                    if selectionList > 0 and name not in selectionList:
                        if name in self.shadersDict:
                            defShader = self.shadersDict[AiNodeGetName(node)]
                            AiNodeSetPtr(node, "shader", defShader)
                        continue

                # Setting overrides
                if name in self.shadersDict:
                    defShader = self.shadersDict[AiNodeGetName(node)]
                    result = {0: lambda: AiNodeSetPtr(node, "shader", defShader),
                              1: lambda: AiNodeSetPtr(node, "shader", self.checkerShader),
                              2: lambda: AiNodeSetPtr(node, "shader", self.greyShader),
                              3: lambda: AiNodeSetPtr(node, "shader", self.mirrorShader),
                              4: lambda: AiNodeSetPtr(node, "shader", self.normalShader),
                              5: lambda: AiNodeSetPtr(node, "shader", self.occlusionShader),
                              6: lambda: AiNodeSetPtr(node, "shader", self.uvShader)}[shaderIndex]()

        # Texture Repeat Udpate
        if attr == None or attr == 5:
            texRepeat = self.textureRepeatSlider.value()
            if ARNOLD_5:
                AiNodeSetVec2(self.placeTexture, "repeatUV", texRepeat, texRepeat)
            else:
                AiNodeSetPnt2(self.placeTexture, "repeatUV", texRepeat, texRepeat)

        if attr == None or attr == 6:
            scanning = self.bucketComboBox.currentName()
            AiNodeSetStr(options, "bucket_scanning", scanning)

        try:
            cmds.arnoldIpr(mode='unpause')
        except RuntimeError:
            pass

    def timeChnaged(self, *args):
        ''' Callback method to update the frame number attr '''
        options = AiUniverseGetOptions()
        time = cmds.currentTime(q=1)
        AiNodeSetFlt(options, "frame", time)

    def selectionChanged(self, *args):
        ''' Callback method to update the frame number attr '''
        shaderIndex = self.shaderComboBox.currentIndex()
        selectedObjects = self.selectedShaderCheckbox.isChecked()
        if shaderIndex > 0 and selectedObjects:
            self.IPRUpdate(4)

    def stop(self):
        ''' Stops the render session and removes the callbacks '''
        if self.timeChangedCB != None:
            OM.MEventMessage.removeCallback(self.timeChangedCB)
            self.timeChangedCB = None

        if self.selectionChangedCB != None:
            OM.MEventMessage.removeCallback(self.selectionChangedCB)
            self.selectionChangedCB = None

        try:
            cmds.arnoldIpr(mode='stop')
            sys.stdout.write("// Info: Aton - Render stopped.\n")
        except (AttributeError, RuntimeError):
            return

        self.frame_sequence.stop()

    def closeEvent(self, event):
        ''' Removes callback when closing the GUI '''
        self.stop()
        self.frame_sequence.stop()
        self.deleteInstances()

        if self.defaultMergeAOVs is not None:
            cmds.setAttr("defaultArnoldDriver.mergeAOVs",  self.defaultMergeAOVs)
        if self.defaultAiTranslator is not None:
            cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", self.defaultAiTranslator, type="string")

    def dockCloseEventTriggered(self):
        self.closeEvent(None)

    def keyPressEvent(self, event):
        if event.key() == QtCore.Qt.Key_Escape:
            self.frame_sequence.stop()

class Signal(set):
    '''Qt Signal Clone allows'''
    connect = set.add
    disconnect = set.discard
    def emit(self, *args, **kwargs):
        for fn in list(self):
            fn(*args, **kwargs)

class AiFrameSequence(object):
    '''
    Step through a batch of frames using the specified timeout in seconds. For
    each frame wait for the frame to start rendering, and then stop rendering
    before moving on. Stop stepping early using the stop method.
    AiFrameSequence emits the following signals: started, stopped, stepped,
    frame_changed. stepped emits the step in the inner frame loop, it can be
    used to report progress. frame_changed emits the frame number when the
    frame is changed.

    usage::

       b = AiFrameSequence(xrange(10, 20, 2), 1)
       b.start()
       # OR LIKE THIS
       b = AiFrameSequence()
       b.frames = xrange(10, 20, 2)
       b.timeout = 1
       b.start()
    '''

    def __init__(self, frames=None, timeout=None):
        self.frames = frames or []
        self.timeout = timeout
        self.running = False
        self.started = Signal()
        self.stopped = Signal()
        self.stepped = Signal()
        self.frame_changed = Signal()

    def change_frame(self, frame):
        cmds.currentTime(frame)
        options = AiUniverseGetOptions()
        AiNodeSetFlt(options, "frame", frame)
        self.frame_changed.emit(frame)

    def start(self):
        '''Start stepping through frames'''
        self.running = True
        self.started.emit()
        gMainProgressBar = mel.eval('$tmp = $gMainProgressBar')

        for i, frame in enumerate(self.frames):
            isCancelled = cmds.progressBar(gMainProgressBar, q=True, ic=True)
            if not self.running or isCancelled:
                break
            self.change_frame(frame)
            self.stepped.emit(i)

            # Sleep until frame starts, then finishes
            sleep_until(conditions = [AiRendering, lambda: not AiRendering()],
                        wake_condition = lambda: not self.running,
                        timeout=self.timeout)

        self.running = False
        self.stopped.emit()

    def stop(self):
        '''Stop stepping through frames'''
        self.running = False

def qt_sleep(secs=0):
    '''Non-blocking sleep for Qt'''
    start = default_timer()

    while True:
        QtWidgets.qApp.processEvents()
        if default_timer() - start > secs:
            return

def sleep_until(conditions, wake_condition=None, timeout=None):
    '''
    Process qApp events until a sequence of conditions becomes True. Return
    when each condition returns True or the wake_condition returns True or
    the timeout is reached...
    :param conditions: Sequence of callables returning True or False
    :param wake_condition: Optional callable returning True or False
    :param timeout: Number of seconds to wait before returning
    '''

    start = default_timer()

    for condition in conditions:
        while True:
            if condition():
                break
            if timeout:
                if default_timer() - start > timeout:
                    break
            if wake_condition():
                break

            qt_sleep(0.1)

if __name__ == "__main__":
    aton = Aton()
    aton.show()
