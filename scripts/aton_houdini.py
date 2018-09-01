__copyright__ = "2018 All rights reserved. See Copyright.txt for more details."
__version__ = "1.3.0"

import os, re
from functools import partial

import hou, htoa

from hutil.Qt import QtCore, QtWidgets, QtGui
from htoa.node.parms import HaNodeSetStr
from htoa.node.node import nodeSetArrayString

from arnold import *

def warn(msg, *params):
    header = '[%s] ' % __name__
    AiMsgWarning(header + msg, *params)

def atonPatch():
    import htoa.session
    # Only monkey patch once -- the arnold.py soho script from HtoA can and
    # typically will be called many times. Only monkey patch (decorate) the
    # generate method once.
    if htoa.object.rop.HaRop.generate.__name__ == 'generate':
        htoa.session.HaRop.generate = generateDecorated(htoa.session.HaRop.generate)

def atonUpdate(self):
    
    if self.session.isInteractiveRender():

        options_node = AiUniverseGetOptions()

        if AiNodeLookUpUserParameter(options_node, 'aton_enable'):
            
            if AiNodeGetBool(options_node, 'aton_enable'):

                aton_node = getAtonDriver(self, "driver_aton", 'aton')

                AiNodeSetStr(aton_node, "host", AiNodeGetStr(options_node, "aton_host"))
                AiNodeSetInt(aton_node, "port", AiNodeGetInt(options_node, "aton_port"))
                AiNodeSetStr(aton_node, "output", AiNodeGetStr(options_node, "aton_output"))
                AiNodeSetStr(options_node, "bucket_scanning", AiNodeGetStr(options_node, "aton_bucket"))
                AiNodeSetInt(options_node, 'region_min_x', AiNodeGetInt(options_node, 'aton_region_min_x'))
                AiNodeSetInt(options_node, 'region_min_y', AiNodeGetInt(options_node, 'aton_region_min_y'))
                AiNodeSetInt(options_node, 'region_max_x', AiNodeGetInt(options_node, 'aton_region_max_x'))
                AiNodeSetInt(options_node, 'region_max_y', AiNodeGetInt(options_node, 'aton_region_max_y'))
                AiNodeSetBool(options_node, 'ignore_motion_blur', AiNodeGetBool(options_node, 'aton_ignore_mbl'))
                AiNodeSetBool(options_node, 'ignore_subdivision', AiNodeGetBool(options_node, 'aton_ignore_sdv'))
                AiNodeSetBool(options_node, 'ignore_displacement', AiNodeGetBool(options_node, 'aton_ignore_dsp'))
                AiNodeSetBool(options_node, 'ignore_bump', AiNodeGetBool(options_node, 'aton_ignore_bmp'))
                AiNodeSetBool(options_node, 'ignore_sss', AiNodeGetBool(options_node, 'aton_ignore_sss'))

                # Get the outputs string array param (on the options node) as a python list
                array = AiNodeGetArray(options_node, 'outputs')
                elements = AiArrayGetNumElements(array)
                outputs = [AiArrayGetStr(array, i) for i in xrange(elements)]

                # RGBA primary should be the first in outputs--its last string
                # should be the node name of the driver_houdini (IPR) main driver
                name = outputs[0].split()[-1]
                aton_outputs = [i.replace(name, AiNodeGetName(aton_node)) for i in outputs if i.endswith(name)]
                nodeSetArrayString(options_node, 'outputs', aton_outputs)
            else:
                warn("Aton is not Enabled!")
        else:
            warn("No Aton driver plug-in node 'driver_aton' could be found installed.")

def generateDecorated(func):
    def generateDecorator(self, *args, **kwargs):
        result = func(self, *args, **kwargs)
        atonUpdate(self)
        return result
    
    return generateDecorator

def getAtonDriver(self, nodeEntryName, newSubStr):
    from htoa.object.camera import cameraTag

    nodeIter = AiUniverseGetNodeIterator(AI_NODE_DRIVER)
    while not AiNodeIteratorFinished(nodeIter):
        node = AiNodeIteratorGetNext(nodeIter)
        nodeEntry = AiNodeGetNodeEntry(node)
        if AiNodeEntryGetName(nodeEntry) == nodeEntryName:
            return node

    driverAtoNNode = AiNode(nodeEntryName)
    camTag = cameraTag(self.session.camera_name)
    HaNodeSetStr(driverAtoNNode, 'name',
                 (self.path + ':' + newSubStr + (':%s' % camTag if camTag else '')))
    return driverAtoNNode

def getHost():
    ''' Returns a host name from Aton driver '''
    aton_host = os.getenv("ATON_HOST")
    
    if (aton_host is None):
        return "127.0.0.1"
    else:
        return aton_host
    
def getPort():
    ''' Returns a port number from Aton driver '''
    aton_port = os.getenv("ATON_PORT")
    
    if aton_port is None:
        return 9201;
    else:
        return int(aton_port)

def getOutputDrivers(path = False):
    ''' Returns a list of all output driver names '''
    rops = hou.nodeType(hou.nodeTypeCategories()['Driver'], 'arnold').instances()
    
    if path:
        return [i.path() for i in rops]
    
    return rops

def getBucketModes(rop):
    if rop:
        parmTemplateGroup = rop.parmTemplateGroup()
        parmTamplateName = 'ar_bucket_scanning'
        parmTemplateExist = parmTemplateGroup.find(parmTamplateName)
        if parmTemplateExist:
            return rop.parm(parmTamplateName).parmTemplate().menuItems()

def getAllCameras(path = False):
    ''' Returns a list of all camera names '''
    cameras = hou.nodeType(hou.nodeTypeCategories()['Object'], 'cam').instances()
    
    if path:
        return [i.path() for i in cameras]
    
    return cameras

def getCurrentCamera(path = False):
    camera = hou.ui.paneTabOfType(hou.paneTabType.SceneViewer).curViewport().camera()
    if camera is not None:
        if path:
            return camera.path()
        else:
            return camera

def getResolution(rop, resX=False, resY=False):
    camera = hou.node(rop.parm('camera').eval())

    if camera:
        if resX:
            return camera.parm('resx').eval()
        elif resY:
            return camera.parm('resy').eval()
    else:
        return 0;


class Output(object):
    def __init__(self, rop=None):
        if rop:
            self.rop = rop
            self._init_attributes()
        else:
            self._init_blank()

    def _init_attributes(self):
        self.name = self.rop.name()
        self.path = self.rop.path()
        self.camera = self.rop.parm('camera').eval()
        self.overrideCameraRes = self.rop.parm('override_camerares').eval()
        self.resFraction = self.rop.parm('res_fraction').eval()
        self.resolution = hou.node(self.camera).parmTuple('res').eval()
        self.AASamples = self.rop.parm('ar_AA_samples').eval()
        self.bucketScanning = self.rop.parm('ar_bucket_scanning').eval()

    def _init_blank(self):
        self.rop = None
        self.name = None
        self.path = None
        self.camera = None
        self.overrideCameraRes = None
        self.resFraction = None
        self.resolution = (0,0)
        self.AASamples = 0
        self.bucketScanning = None
    
    def rollback(self):
        hou.node(self.camera).parmTuple('res').set(self.resolution)
        self.rop.parm('camera').set(self.camera)
        self.rop.parm('res_fraction').set(self.resFraction)
        self.rop.parm('override_camerares').set(self.overrideCameraRes)
        self.rop.parm('ar_AA_samples').set(self.AASamples)
        self.rop.parm('ar_bucket_scanning').set(self.bucketScanning)

class BoxWidget(QtWidgets.QFrame):
        def __init__(self, label, first=True):
            super(BoxWidget, self).__init__()
            self.label = QtWidgets.QLabel(label + ":")
            self.label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignCenter)

            if first:
                    self.label.setMinimumSize(75, 20)
                    self.label.setMaximumSize(75, 20)

            self.layout = QtWidgets.QHBoxLayout(self)
            self.layout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
            self.layout.setContentsMargins(0, 0, 0, 0)
            self.layout.addWidget(self.label)

class LineEditBox(BoxWidget):
        def __init__(self, label, text='', first=True):
            super(LineEditBox, self).__init__(label, first)

            self.lineEditBox = QtWidgets.QLineEdit()
            self.lineEditBox.setText(u"%s" % text)
            self.layout.addWidget(self.lineEditBox)

        def setEnabled(self, value):
            self.label.setEnabled(value)
            self.lineEditBox.setEnabled(value)

        def text(self):
            return self.lineEditBox.text()

        def setText(self, text):
            self.lineEditBox.setText(text)

class SliderBox(BoxWidget):
        def __init__(self, label, value=0, first=True):
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
            self.spinBox.setRange(-99999, 99999)
        
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
            self.comboBox.setSizePolicy(QtWidgets.QSizePolicy.Expanding,
                                        QtWidgets.QSizePolicy.Fixed)
            self.currentIndexChanged = self.comboBox.currentIndexChanged

            self.layout.addWidget(self.comboBox)

        def setEnabled(self, value):
            self.label.setEnabled(value)
            self.comboBox.setEnabled(value)

        def setCurrentIndex(self, value):
            self.comboBox.setCurrentIndex(value)


        def setCurrentName(self, value):
            for idx, item in enumerate(self.items):
                if item == value:
                    self.comboBox.setCurrentIndex(idx)

        def currentIndex(self):
            return self.comboBox.currentIndex()

        def currentName(self):
            index = self.comboBox.currentIndex()
            if self.items:
                return self.items[index]

        def addItems(self, items):
            if items:
                for i in items:
                    self.comboBox.addItem(i)
                self.items += items

        def clear(self):
            self.comboBox.clear()
            self.items = []

class Aton(QtWidgets.QWidget):
    
    def __init__(self):
        QtWidgets.QWidget.__init__(self)

        # Properties
        self._ipr = None
        self._output = None
        self._user_options = None
        
        # Default Settings
        self.defaultPort = getPort()
        self.defaultHost = getHost()
        self.outputsList = [Output(rop) for rop in getOutputDrivers()]

        # Init UI
        self.objName = self.__class__.__name__.lower()
        self.setObjectName(self.objName)
        self.setWindowTitle(self.__class__.__name__)
        self.setProperty("saveWindowPref", True)
        self.setProperty("houdiniStyle", True)
        self.setStyleSheet(hou.qt.styleSheet())

        # Setup UI
        self.deleteInstances()
        self.setupUI()
    
    @property
    def ipr(self):
        if self._ipr is None:
            desk = hou.ui.curDesktop()
            self._ipr = desk.paneTabOfType(hou.paneTabType.IPRViewer)
        return self._ipr

    @property
    def user_options(self):
        if self._user_options is None:
            # Check if it's an Arnold ROP
            userOptionsParmName = 'ar_user_options'
            rop_node = hou.node(self.outputComboBox.currentName())
            parmTemplateGroup = rop_node.parmTemplateGroup()
            parmTemplate = parmTemplateGroup.find(userOptionsParmName)

            if parmTemplate:
                self._user_options = rop_node.parm(userOptionsParmName)

        return self._user_options

    @property
    def output(self):
        idx  = self.outputComboBox.currentIndex()
        if idx >= 0:
            self._output = self.outputsList[idx]
        else:
            self._output = Output()
        return self._output

    def deleteInstances(self):
        for w in QtWidgets.QApplication.instance().topLevelWidgets():
            if w.objectName() == self.objName:
                w.close()
                w.destroy()
    
    def closeEvent(self, event):
        self.setParent(None)
        try:
            self.removeAtonOverrides()
        except AttributeError:
            pass

        try:
            self.ipr.killRender()
        except AttributeError:
            pass

    def setupUI(self):

        def outputUpdate(index):
            if index >= 0:
                self.cameraComboBox.setCurrentName(self.output.camera)
                self.bucketComboBox.setCurrentName(self.output.bucketScanning)
                resUpdateUI(self.resolutionSlider.slider.value())
                self.cameraAaSlider.setValue(self.output.AASamples)
                self.renderRegionRSpinBox.setValue(self.output.resolution[0])
                self.renderRegionTSpinBox.setValue(self.output.resolution[1])
        
        def portUpdateUI(value):
            self.portSlider.spinBox.setValue(value + self.defaultPort)

        def resUpdateUI(value):
            self.resolutionSlider.setValue(value * 5)
            xres = self.output.resolution[0] * value * 5 / 100
            yres = self.output.resolution[1] * value * 5 / 100
            resolutionInfoLabel.setText("%dx%d"%(xres, yres))

        def resetUI(*args):
            self.outputsList = [Output(rop) for rop in getOutputDrivers()]

            self.hostCheckBox.setChecked(True)
            self.portCheckBox.setChecked(True)
            self.hostLineEdit.setText(self.defaultHost)
            self.portSlider.setValue(self.defaultPort, 0)
            self.outputComboBox.clear()
            self.outputComboBox.addItems(getOutputDrivers(path=True))
            self.resolutionSlider.setValue(100, 20)
            self.renderRegionXSpinBox.setValue(0)
            self.renderRegionYSpinBox.setValue(0)
            self.overscanSlider.setValue(0, 0)
            self.motionBlurCheckBox.setChecked(False)
            self.subdivsCheckBox.setChecked(False)
            self.displaceCheckBox.setChecked(False)
            self.bumpCheckBox.setChecked(False)
            self.sssCheckBox.setChecked(False)

        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setAttribute(QtCore.Qt.WA_AlwaysShowToolTips)

        # Main Layout
        mainLayout = QtWidgets.QVBoxLayout()
        
        # General Group
        generalGroupBox = QtWidgets.QGroupBox("General")
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

        # Output Driver Layout
        outputDriverLayout = QtWidgets.QHBoxLayout()
        self.outputComboBox = ComboBox("Output")
        self.outputComboBox.addItems([i.path for i in self.outputsList])
        self.outputComboBox.currentIndexChanged.connect(outputUpdate)
        outputDriverLayout.addWidget(self.outputComboBox)

        # Camera Layout
        cameraLayout = QtWidgets.QHBoxLayout()
        self.cameraComboBox = ComboBox("Camera")
        self.cameraComboBox.addItems(getAllCameras(path=True))
        self.cameraComboBox.setCurrentName(self.output.camera)
        cameraLayout.addWidget(self.cameraComboBox)

        # Overrides Group
        overridesGroupBox = QtWidgets.QGroupBox("Overrides")
        overridesLayout = QtWidgets.QVBoxLayout(overridesGroupBox)

        # Bucket Layout
        bucketLayout = QtWidgets.QHBoxLayout()
        self.bucketComboBox = ComboBox("Bucket Scan")
        self.bucketComboBox.addItems(getBucketModes(self.output.rop))
        self.bucketComboBox.setCurrentName(self.output.bucketScanning)
        bucketLayout.addWidget(self.bucketComboBox)

        # Resolution Layout
        resolutionLayout = QtWidgets.QHBoxLayout()
        self.resolutionSlider = SliderBox("Resolution %")
        self.resolutionSlider.setMinimum(1, 1)
        self.resolutionSlider.setMaximum(200, 40)
        self.resolutionSlider.setValue(100, 20)
        self.resolutionSlider.connect(resUpdateUI)
        xres, yres = self.output.resolution[0], self.output.resolution[1]
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
        self.cameraAaSlider.setValue(self.output.AASamples)
        cameraAaLayout.addWidget(self.cameraAaSlider)

        # Render region layout
        renderRegionLayout = QtWidgets.QHBoxLayout()
        self.renderRegionXSpinBox = SpinBox("Region X")
        self.renderRegionYSpinBox = SpinBox("Y", 0, False)
        self.renderRegionRSpinBox = SpinBox("R", 0, False)
        self.renderRegionTSpinBox = SpinBox("T", 0, False)
        self.renderRegionRSpinBox.setValue(self.output.resolution[0])
        self.renderRegionTSpinBox.setValue(self.output.resolution[1])
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

        # Ignore Group
        ignoresGroupBox = QtWidgets.QGroupBox("Ignore")
        ignoresGroupBox.setMaximumSize(9999, 75)

        # Ignore Layout
        ignoresLayout = QtWidgets.QVBoxLayout(ignoresGroupBox)
        ignoreLayout = QtWidgets.QHBoxLayout()
        ignoreLabel = QtWidgets.QLabel("Ignore:")
        ignoreLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        self.motionBlurCheckBox = QtWidgets.QCheckBox("Motion Blur")
        self.subdivsCheckBox = QtWidgets.QCheckBox("Subdivs")
        self.displaceCheckBox = QtWidgets.QCheckBox("Displace")
        self.bumpCheckBox = QtWidgets.QCheckBox("Bump")
        self.sssCheckBox = QtWidgets.QCheckBox("SSS")
        ignoreLayout.addWidget(self.motionBlurCheckBox)
        ignoreLayout.addWidget(self.subdivsCheckBox)
        ignoreLayout.addWidget(self.displaceCheckBox)
        ignoreLayout.addWidget(self.bumpCheckBox)
        ignoreLayout.addWidget(self.sssCheckBox)

        # Main Buttons Layout
        mainButtonslayout = QtWidgets.QHBoxLayout()
        startButton = QtWidgets.QPushButton("Start / Refresh")
        stopButton = QtWidgets.QPushButton("Stop")
        resetButton = QtWidgets.QPushButton("Reset")
        startButton.clicked.connect(self.startRender)
        stopButton.clicked.connect(self.stopRender)
        resetButton.clicked.connect(resetUI)
        mainButtonslayout.addWidget(startButton)
        mainButtonslayout.addWidget(stopButton)
        mainButtonslayout.addWidget(resetButton)

        # Add Layouts to Main
        generalLayout.addLayout(hostLayout)
        generalLayout.addLayout(portLayout)
        generalLayout.addLayout(outputDriverLayout)
        overridesLayout.addLayout(cameraLayout)
        overridesLayout.addLayout(bucketLayout)
        overridesLayout.addLayout(cameraAaLayout)
        overridesLayout.addLayout(resolutionLayout)
        overridesLayout.addLayout(renderRegionLayout)
        overridesLayout.addLayout(overscanLayout)
        ignoresLayout.addLayout(ignoreLayout)

        mainLayout.addWidget(generalGroupBox)
        mainLayout.addWidget(overridesGroupBox)
        mainLayout.addWidget(ignoresGroupBox)
        mainLayout.addLayout(mainButtonslayout)

        self.cameraComboBox.currentIndexChanged.connect(lambda : self.addAtonOverrides(0))
        self.bucketComboBox.currentIndexChanged.connect(lambda : self.addAtonOverrides(3))
        self.cameraAaSlider.valueChanged.connect(lambda : self.addAtonOverrides(1))
        self.resolutionSlider.valueChanged.connect(lambda : self.addAtonOverrides(2))
        self.renderRegionXSpinBox.valueChanged.connect(lambda: self.addAtonOverrides(2))
        self.renderRegionYSpinBox.valueChanged.connect(lambda: self.addAtonOverrides(2))
        self.renderRegionRSpinBox.valueChanged.connect(lambda: self.addAtonOverrides(2))
        self.renderRegionTSpinBox.valueChanged.connect(lambda: self.addAtonOverrides(2))
        self.overscanSlider.valueChanged.connect(lambda : self.addAtonOverrides(2))
        self.motionBlurCheckBox.toggled.connect(lambda: self.addAtonOverrides(4))
        self.subdivsCheckBox.toggled.connect(lambda: self.addAtonOverrides(4))
        self.displaceCheckBox.toggled.connect(lambda: self.addAtonOverrides(4))
        self.bumpCheckBox.toggled.connect(lambda: self.addAtonOverrides(4))
        self.sssCheckBox.toggled.connect(lambda: self.addAtonOverrides(4))

        self.setLayout(mainLayout)

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

    def getRegion(self, attr, resScale = True):
        if resScale:
            resValue = self.resolutionSlider.value()
        else:
            resValue = 100

        ovrScnValue = self.overscanSlider.value() * resValue / 100

        xres = self.output.resolution[0] * resValue / 100
        yres = self.output.resolution[1] * resValue / 100

        result = {0 : lambda: xres,
                  1 : lambda: yres,
                  2 : lambda: (self.renderRegionXSpinBox.value() * resValue / 100) - ovrScnValue,
                  3 : lambda: yres - (self.renderRegionTSpinBox.value() * resValue / 100) - ovrScnValue,
                  4 : lambda: (self.renderRegionRSpinBox.value() * resValue / 100) - 1 + ovrScnValue,
                  5 : lambda: (yres - (self.renderRegionYSpinBox.value() * resValue / 100)) - 1 + ovrScnValue}[attr]()

        return result

    def addAtonOverrides(self, mode=None):
        if self.ipr.isActive():
            userOptions = self.removeAtonOverrides()
            
            # Aton Attributes
            userOptions += ' ' if userOptions else ''
            userOptions += 'declare aton_enable constant BOOL aton_enable off '
            userOptions += 'declare aton_host constant STRING aton_host %s '%self.hostLineEdit.text()
            userOptions += 'declare aton_port constant INT aton_port %d '%self.portSlider.value()
            userOptions += 'declare aton_output constant STRING aton_output %s '%self.output.name

            # Camera
            if mode == None or mode == 0:
                self.output.rop.parm('camera').set(self.cameraComboBox.currentName())
            
            # AA Samples
            if mode == None or mode == 1:
                self.output.rop.parm('ar_AA_samples').set(self.cameraAaSlider.value())

            # Resolution
            if mode == None or mode == 2:
                self.output.rop.parm('override_camerares').set(True)
                self.output.rop.parm('res_fraction').set('specific')
                self.output.rop.parm('res_overridex').set(self.getRegion(0))
                self.output.rop.parm('res_overridey').set(self.getRegion(1))
            
                # Render Region
                userOptions += 'declare aton_region_min_x constant INT aton_region_min_x %d '%self.getRegion(2)
                userOptions += 'declare aton_region_min_y constant INT aton_region_min_y %d '%self.getRegion(3)
                userOptions += 'declare aton_region_max_x constant INT aton_region_max_x %d '%self.getRegion(4)
                userOptions += 'declare aton_region_max_y constant INT aton_region_max_y %d '%self.getRegion(5)

            # Bucket Scanning
            if mode == None or mode == 3:
                userOptions += 'declare aton_bucket constant STRING aton_bucket %s '%self.bucketComboBox.currentName()

            # Ignore Feautres
            if mode == None or mode == 4:
                userOptions += 'declare aton_ignore_mbl constant BOOL aton_ignore_mbl %s '%('on' if self.motionBlurCheckBox.isChecked() else 'off')
                userOptions += 'declare aton_ignore_sdv constant BOOL aton_ignore_sdv %s '%('on' if self.subdivsCheckBox.isChecked() else 'off')
                userOptions += 'declare aton_ignore_dsp constant BOOL aton_ignore_dsp %s '%('on' if self.displaceCheckBox.isChecked() else 'off')
                userOptions += 'declare aton_ignore_bmp constant BOOL aton_ignore_bmp %s '%('on' if self.bumpCheckBox.isChecked() else 'off')
                userOptions += 'declare aton_ignore_sss constant BOOL aton_ignore_sss %s '%('on' if self.sssCheckBox.isChecked() else 'off')

            self.user_options.set(userOptions)

    def removeAtonOverrides(self):
        for output in self.outputsList:
            output.rollback()
        
        userOptions = self.user_options.evalAsString()
        userOptions = re.sub('declare aton_enable.+', '', userOptions)
        self.user_options.set(userOptions)
        return userOptions

    def isNotActive(self, ipr):
        return not ipr.isActive()        

    def startRender(self):

        if self.output:
            
            # Set IPR Options
            self.ipr.setPreview(True)  
            self.ipr.setAutoUpdate(True)
            self.ipr.killRender()
            self.ipr.setRopNode(self.output.rop)
            self.ipr.startRender()
            
            self.addAtonOverrides()

            # Wait until IPR is no longer active
            hou.ui.waitUntil(partial(self.isNotActive, ipr=self.ipr)) 
            self.removeAtonOverrides()

    def stopRender(self):
        if self.output:
            self.ipr.killRender()

