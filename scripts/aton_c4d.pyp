__author__ = "Peter Horvath, Vahan Sosoyan"
__copyright__ = "2019 All rights reserved. See Copyright.txt for more details."
__version__ = "1.3.5"

import c4d, os
from c4d import bitmaps, documents, gui, plugins

# Aton_c4d Plugin ID
PLUGIN_ID = 1038628

ATON_HOST = 464911602
ATON_PORT = 464624139
ATON_OUTPUT = 798505185

# from c4dtoa_symols.h
ARNOLD_RENDER_OVERRIDES = 1038579
ARNOLD_DRIVER = 1030141
ARNOLD_RENDER_COMMAND = 1038578  # 1: Start IPR, 2: Stop IPR, 3: Pause/Unpause, 4: Trigger to refresh 5: Render sequence

# from arnold_renderer.h
C4DTOA_RENDEROVERRIDE_DRIVER = 1000                 # [Link] The driver object used as the display driver. The default display driver is used when does not set.
C4DTOA_RENDEROVERRIDE_ONLY_BEAUTY = 1001            # [Bool] If true then no AOVs of the display driver are exported. Default is true.
C4DTOA_RENDEROVERRIDE_EXPORT_ALL_DRIVERS = 1002     # [Bool] If false then only the display driver is exported. Default is true.
C4DTOA_RENDEROVERRIDE_CAMERA = 1003                 # [Link] The camera object used for rendering. The active camera is used when does not set.
C4DTOA_RENDEROVERRIDE_XRES = 1004                   # [Int32] Resolution width.
C4DTOA_RENDEROVERRIDE_YRES = 1005                   # [Int32] Resolution height.
C4DTOA_RENDEROVERRIDE_USE_REGION = 1006             # [Bool] If true then a region is rendered.
C4DTOA_RENDEROVERRIDE_REGION_X1 = 1007              # [Int32] Left border of the render region.
C4DTOA_RENDEROVERRIDE_REGION_X2 = 1008              # [Int32] Right border of the render region.
C4DTOA_RENDEROVERRIDE_REGION_Y1 = 1009              # [Int32] Top border of the render region.
C4DTOA_RENDEROVERRIDE_REGION_Y2 = 1010              # [Int32] Bottom border of the render region.
C4DTOA_RENDEROVERRIDE_AA_SAMPLES = 1011             # [Int32] Camera AA samples.
C4DTOA_RENDEROVERRIDE_IGNORE_MOTION_BLUR = 1012     # [Bool] Ignore motion blur flag.
C4DTOA_RENDEROVERRIDE_IGNORE_SUBDIV = 1013          # [Bool] Ignore subdivision flag.
C4DTOA_RENDEROVERRIDE_IGNORE_DISPLACEMENT = 1014    # [Bool] Ignore displacement flag.
C4DTOA_RENDEROVERRIDE_IGNORE_BUMP = 1015            # [Bool] Ignore bump flag.
C4DTOA_RENDEROVERRIDE_IGNORE_SSS = 1016             # [Bool] Ignore SSS flag.
C4DTOA_RENDEROVERRIDE_FRAME_START = 1017            # [Int32] Start frame when rendering an animation.
C4DTOA_RENDEROVERRIDE_FRAME_END = 1018              # [Int32] End frame when rendering an animation.
C4DTOA_RENDEROVERRIDE_FRAME_STEP = 1019             # [Int32] Frame step when rendering an animation.
C4DTOA_RENDEROVERRIDE_PROGRESSIVE_REFINEMENT = 1020 # [Bool] Enable / disable progressive refinement in the IPR.
C4DTOA_RENDEROVERRIDE_PROGRESSIVE_SAMPLES = 1021    # [Int32] Level of progressive refinement (e.g. -3).

C4DAIN_DRIVER_ATON = 313785406

class Aton(gui.GeDialog):
    # UI Lengths
    LabelWidth = 85
    LabelHight = 10

    # UI IDs
    PortID = 100
    HostID = 110
    CameraID = 120
    ResolutionID = 130
    ResolutionInfoID = 131
    CameraAAID = 140
    RenderRegionXID = 150
    RenderRegionYID = 151
    RenderRegionRID = 152
    RenderRegionTID = 153
    RenderRegionGetID = 154
    OverscanID = 160
    OverscanSetID = 161
    MotionBlurID = 170
    SubdivID = 171
    DisplaceID = 172
    BumpID = 173
    SSSID = 174
    SequenceOnID = 180
    SequenceStartID = 181
    SequenceEndID = 182
    SequenceStepID = 183
    SequenceStartLabelID = 184
    SequenceEndLabelID = 185
    SequenceStepLabelID = 186
    StartButtonID = 190
    StopButtonID = 191
    ResetButtonID = 192
    LabelID = 1000

    def initilize(self):
        '''Main initilaizer'''
        self.objName = self.__class__.__name__
        self.createDriver(self.objName)
        self.defaultHost = self.getSceneOption(0)
        self.defaultPort = self.getSceneOption(1)
        self.cameras = self.getSceneOption(2)

    def CreateLayout(self):
        '''Creating Layout UI'''

        def GenLabelID():
            '''Generates incremental LabelID'''
            self.LabelID += 1
            return self.LabelID

        def StartGroup(label):
            self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 1, 1, label)
            self.GroupBorder(c4d.BORDER_GROUP_TOP)
            self.GroupBorderSpace(6, 6, 6, 6)

        def EndGroup():
            self.GroupEnd()

        def AddSliderGroup(id=GenLabelID(), label="", value=0, min=0, max=0, offset=0):
            self.AddStaticText(GenLabelID(), 0, self.LabelWidth, 0, label)
            self.AddEditSlider(id, c4d.BFH_SCALEFIT, 50, self.LabelHight)
            self.SetInt32(id, value, min+offset, max+offset)

        def AddTextGroup(id, label):
            self.AddStaticText(GenLabelID(), 0, self.LabelWidth, 0, label)
            self.AddEditText(id, c4d.BFH_SCALEFIT, 0, self.LabelHight)

        self.initilize()
        self.SetTitle(self.objName)
        self.GroupBorderSpace(8, 8, 8, 8)

        StartGroup("General")

        # Host
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 2, 1)
        AddTextGroup(self.HostID, "Host:")
        self.SetString(self.HostID, self.defaultHost)
        self.GroupEnd()

        # Port
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 2, 1, 0)
        AddSliderGroup(self.PortID, "Port:", self.defaultPort, 1, 16, self.defaultPort-1)
        self.GroupEnd()

        # Camera
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 2, 1)
        self.AddStaticText(GenLabelID(), 0, self.LabelWidth, 0, "Camera:")
        self.AddComboBox(self.CameraID, c4d.BFH_SCALEFIT)
        self.AddChild(self.CameraID, 0, "Current")
        subID = 1
        for i in reversed(self.cameras):
            self.AddChild(self.CameraID, subID, i.GetName())
            subID += 1
        self.GroupEnd()
        EndGroup()

        StartGroup("Overrides")

        # Resolution
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 3, 1, 0)
        AddSliderGroup(self.ResolutionID, "Resolution %:", 100, 1, 200)
        self.AddStaticText(self.ResolutionInfoID, 0, 75, 0, "%sx%s"%(self.getSceneOption(3),
                                                                     self.getSceneOption(4)))
        self.Enable(self.ResolutionInfoID, False)
        self.GroupEnd()

        # Camera AA
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 2, 1, 0)
        AddSliderGroup(self.CameraAAID, "Camera  AA:", 3, -3, 16)
        self.GroupEnd()

        # Render Region
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 9, 1, 0)
        self.AddStaticText(GenLabelID(), 0, self.LabelWidth, 0, "Region X:")
        self.AddEditNumber(self.RenderRegionXID, c4d.BFH_SCALEFIT, 50, self.LabelHight)
        self.AddStaticText(GenLabelID(), 0, 0, 0, "Y:")
        self.AddEditNumber(self.RenderRegionYID, c4d.BFH_SCALEFIT, 50, self.LabelHight)
        self.AddStaticText(GenLabelID(), 0, 0, 0, "R:")
        self.AddEditNumber(self.RenderRegionRID, c4d.BFH_SCALEFIT, 50, self.LabelHight)
        self.AddStaticText(GenLabelID(), 0, 0, 0, "T:")
        self.AddEditNumber(self.RenderRegionTID, c4d.BFH_SCALEFIT, 50, self.LabelHight)
        self.AddButton(self.RenderRegionGetID, 0, 0, 0, "Get")
        self.SetInt32(self.RenderRegionXID, 0)
        self.SetInt32(self.RenderRegionYID, 0)
        self.SetInt32(self.RenderRegionRID, self.getSceneOption(3))
        self.SetInt32(self.RenderRegionTID, self.getSceneOption(4))
        self.GroupEnd()

        # Overscan
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 3, 1, 0)
        AddSliderGroup(self.OverscanID, "Overscan:", 0, 0, 512)
        #self.AddButton(self.OverscanSetID, 0, 0, 0, "Set")
        self.GroupEnd()
        EndGroup()

        StartGroup("Ignore")

        # Ignore
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT, 5, 1)
        self.AddCheckbox(self.MotionBlurID, c4d.BFH_SCALEFIT, self.LabelWidth, 0, "Motion Blur")
        self.AddCheckbox(self.SubdivID, c4d.BFH_SCALEFIT, self.LabelWidth, 0, "Subdiv")
        self.AddCheckbox(self.DisplaceID, c4d.BFH_SCALEFIT, self.LabelWidth, 0, "Displace")
        self.AddCheckbox(self.BumpID, c4d.BFH_SCALEFIT, self.LabelWidth, 0, "Bump")
        self.AddCheckbox(self.SSSID, c4d.BFH_SCALEFIT, self.LabelWidth, 0, "SSS")
        self.GroupEnd()
        EndGroup()

        StartGroup("Sequence")

        # Sequence
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 7, 1, 0)
        self.AddCheckbox(self.SequenceOnID, 0, 0, 0, "")
        self.AddStaticText(self.SequenceStartLabelID, 0, self.LabelWidth, 0, "Start Frame:")
        self.AddEditNumber(self.SequenceStartID, c4d.BFH_SCALEFIT, 50, self.LabelHight)
        self.AddStaticText(self.SequenceEndLabelID, 0, self.LabelWidth, 0, "End Frame:")
        self.AddEditNumber(self.SequenceEndID, c4d.BFH_SCALEFIT, 50, self.LabelHight)
        self.AddStaticText(self.SequenceStepLabelID, 0, self.LabelWidth, 0, "By Frame:")
        self.AddEditNumber(self.SequenceStepID, c4d.BFH_SCALEFIT, 50, self.LabelHight)
        self.SetBool(self.SequenceOnID, False)
        self.Enable(self.SequenceStartID, False)
        self.Enable(self.SequenceEndID, False)
        self.Enable(self.SequenceStepID, False)
        self.Enable(self.SequenceStartLabelID, False)
        self.Enable(self.SequenceEndLabelID, False)
        self.Enable(self.SequenceStepLabelID, False)
        self.SetInt32(self.SequenceStartID, self.getSceneOption(5))
        self.SetInt32(self.SequenceEndID, self.getSceneOption(6))
        self.SetInt32(self.SequenceStepID, self.getSceneOption(7))
        EndGroup()

        # Main Buttuns
        self.GroupBegin(GenLabelID(), c4d.BFH_SCALEFIT|c4d.BFV_SCALEFIT, 3, 1, "")
        self.AddButton(self.StartButtonID, c4d.BFH_SCALEFIT, 100, self.LabelHight+5, "Start / Refresh")
        self.AddButton(self.StopButtonID, c4d.BFH_SCALEFIT, 100, self.LabelHight+5, "Stop")
        self.AddButton(self.ResetButtonID, c4d.BFH_SCALEFIT, 100, self.LabelHight+5, "Reset")
        self.GroupEnd()

        return True

    def resetLayout(self):
        '''Resets Layout to default state'''
        self.SetString(self.HostID, self.defaultHost)
        self.SetInt32(self.PortID, self.defaultPort, self.defaultPort, self.defaultPort+15)
        self.SetInt32(self.CameraID, 0)
        self.SetInt32(self.ResolutionID, 100, 1, 200)
        self.resInfoUpdate(100)
        self.SetInt32(self.CameraAAID, 3, -3, 16)
        self.SetInt32(self.RenderRegionXID, 0)
        self.SetInt32(self.RenderRegionYID, 0)
        self.SetInt32(self.RenderRegionRID, self.getSceneOption(3))
        self.SetInt32(self.RenderRegionTID, self.getSceneOption(4))
        self.SetInt32(self.OverscanID, 0, 0, 500)
        self.SetBool(self.MotionBlurID, False)
        self.SetBool(self.SubdivID, False)
        self.SetBool(self.DisplaceID, False)
        self.SetBool(self.BumpID, False)
        self.SetBool(self.SSSID, False)
        self.SetBool(self.SequenceOnID, False)
        self.Enable(self.SequenceStartID, False)
        self.Enable(self.SequenceEndID, False)
        self.Enable(self.SequenceStepID, False)
        self.Enable(self.SequenceStartLabelID, False)
        self.Enable(self.SequenceEndLabelID, False)
        self.Enable(self.SequenceStepLabelID, False)
        self.SetInt32(self.SequenceStartID, self.getSceneOption(5))
        self.SetInt32(self.SequenceEndID, self.getSceneOption(6))
        self.SetInt32(self.SequenceStepID, self.getSceneOption(7))

    def DestroyWindow(self):
        c4d.CallCommand(ARNOLD_RENDER_COMMAND, 2)
        self.removeDriver()

    def logMessage(self, msg, type=0):
        '''Log Messages Handling'''
        name = self.objName.upper()
        if type == 0:
            log = "%s | INFO: %s."%(name, msg)
        if type == 1:
            log = "%s | WARNING: %s."%(name, msg)
        if type == 2:
            log = "%s | ERROR: %s."%(name, msg)
        print log

    def createDriver(self, name):
        '''Creates driver object in the scene'''
        doc = documents.GetActiveDocument()
        self.driver = doc.SearchObject(name)
        if self.driver is None:
            self.driver = c4d.BaseObject(ARNOLD_DRIVER)
            doc.InsertObject(self.driver)
            self.driver.GetDataInstance().SetInt32(c4d.C4DAI_DRIVER_TYPE, C4DAIN_DRIVER_ATON)
            self.driver.SetName(name)

    def removeDriver(self):
        '''Removes the driver object'''
        doc = documents.GetActiveDocument()
        doc.SetSelection(self.driver)
        c4d.CallCommand(100004787)

    def getSceneOption(self, attr):
        '''Get Scence options'''
        result = 0
        doc = documents.GetActiveDocument()
        data = doc.GetActiveRenderData()
        try:
            result = {0: lambda : self.getHost(),
                      1: lambda : self.getPort(),
                      2: lambda : self.getCameras(),
                      3: lambda : int(data[c4d.RDATA_XRES]),
                      4: lambda : int(data[c4d.RDATA_YRES]),
                      5: lambda : int(data[c4d.RDATA_FRAMEFROM].Get() * data[c4d.RDATA_FRAMERATE]),
                      6: lambda : int(data[c4d.RDATA_FRAMETO].Get() * data[c4d.RDATA_FRAMERATE]),
                      7: lambda : int(data[c4d.RDATA_FRAMESTEP])}[attr]()
        except ValueError:
            pass
        return result

    def getRegion(self, attr, resScale = True):
        '''Returns Region / Overscan values'''
        if resScale:
            resValue = self.GetInt32(self.ResolutionID)
        else:
            resValue = 100

        # ovrScnValue = 0
        ovrScnValue = self.GetInt32(self.OverscanID) * resValue / 100

        xres = self.getSceneOption(3) * resValue / 100
        yres = self.getSceneOption(4) * resValue / 100

        renderRegionX = self.GetInt32(self.RenderRegionXID)
        renderRegionY = self.GetInt32(self.RenderRegionYID)
        renderRegionR = self.GetInt32(self.RenderRegionRID)
        renderRegionT = self.GetInt32(self.RenderRegionTID)

        result = {0 : lambda: xres,
                  1 : lambda: yres,
                  2 : lambda: (renderRegionX * resValue / 100) - ovrScnValue,
                  3 : lambda: yres - (renderRegionT * resValue / 100) - ovrScnValue,
                  4 : lambda: (renderRegionR * resValue / 100) - 1 + ovrScnValue,
                  5 : lambda: (yres - (renderRegionY * resValue / 100)) - 1 + ovrScnValue}[attr]()

        return result

    def getHost(self):
        ''' Returns the host from Aton driver '''
        host = 0
        try: # To init Arnold Render settings
            host = self.driver.GetDataInstance().GetString(ATON_HOST)
        except ValueError:
            pass
        return host

    def getPort(self):
        ''' Returns the port number from Aton driver '''
        port = 0
        try: # To init Arnold Render settings
            port = self.driver.GetDataInstance().GetInt32(ATON_PORT)
        except ValueError:
            pass
        return port

    def getCameras(self):
        '''Get the list of Cameras in the scene'''
        c4d.CallCommand(12112, 12112)
        doc = documents.GetActiveDocument()
        cams = doc.GetActiveObjectsFilter(True, c4d.Ocamera, c4d.NOTOK)
        c4d.CallCommand(12113, 12113)
        return cams

    def getNukeCropNode(self, *args):
        ''' Get crop node data from Nuke '''
        def find_between(s, first, last):
            try:
                start = s.index(first) + len(first)
                end = s.index(last, start)
                return s[start:end]
            except ValueError:
                return ""

        data = c4d.GetStringFromClipboard()

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

            self.SetInt32(self.RenderRegionXID, nkX)
            self.SetInt32(self.RenderRegionYID, nkY)
            self.SetInt32(self.RenderRegionRID, nkR)
            self.SetInt32(self.RenderRegionTID, nkT)

    def setOverscan(self):
        '''Sets the overscan values in to the scene Render Settings'''
        self.logMessage("Setting overscan in Render Settings is not supported in C4DtoA", 1)

    def resInfoUpdate(self, value):
        '''Updates UI Resolution info'''
        xres = self.getSceneOption(3) * value / 100
        yres = self.getSceneOption(4) * value / 100
        self.SetString(self.ResolutionInfoID, "%sx%s"%(xres, yres))

    def Command(self, id, msg):
        # UI update for Resolution info
        if id == self.ResolutionID:
            self.resInfoUpdate(self.GetInt32(id))

        # IPR Update
        if id == self.CameraID:
            self.updateIPR(0)
        if id == self.ResolutionID:
            self.updateIPR(1)
        if id == self.CameraAAID:
            self.updateIPR(2)
        if id == self.RenderRegionXID:
            self.updateIPR(1)
        if id == self.RenderRegionYID:
            self.updateIPR(1)
        if id == self.RenderRegionRID:
            self.updateIPR(1)
        if id == self.RenderRegionTID:
            self.updateIPR(1)
        if id == self.OverscanID:
            self.updateIPR(1)
        if id == self.MotionBlurID:
            self.updateIPR(3)
        if id == self.SubdivID:
            self.updateIPR(3)
        if id == self.DisplaceID:
            self.updateIPR(3)
        if id == self.BumpID:
            self.updateIPR(3)
        if id == self.SSSID:
            self.updateIPR(3)

        # Buttun Commands
        if id == self.RenderRegionGetID:
            self.getNukeCropNode()
            self.updateIPR(1)
        if id == self.OverscanSetID:
            self.setOverscan()
        if id == self.SequenceOnID:
            self.Enable(self.SequenceStartID, self.GetBool(self.SequenceOnID))
            self.Enable(self.SequenceEndID, self.GetBool(self.SequenceOnID))
            self.Enable(self.SequenceStepID, self.GetBool(self.SequenceOnID))
            self.Enable(self.SequenceStartLabelID, self.GetBool(self.SequenceOnID))
            self.Enable(self.SequenceEndLabelID, self.GetBool(self.SequenceOnID))
            self.Enable(self.SequenceStepLabelID, self.GetBool(self.SequenceOnID))

        if id == self.StartButtonID:
            self.startIPR()
        if id == self.StopButtonID:
            self.stopIPR()
        if id == self.ResetButtonID:
            self.resetLayout()
            self.updateIPR()

        return True

    def startIPR(self):
        '''Main Function to Start Rendering'''

        # Get doc object
        doc = documents.GetActiveDocument()

        # Updating host and port from UI
        if self.defaultHost != 0 and self.defaultPort != 0:
            port = self.GetInt32(self.PortID)
            host = self.GetString(self.HostID)
            take = doc.GetTakeData().GetCurrentTake().GetName()
            self.driver.GetDataInstance().SetInt32(ATON_PORT, port)
            self.driver.GetDataInstance().SetString(ATON_HOST, host)
            self.driver.GetDataInstance().SetString(ATON_OUTPUT, take)
        else:
            self.logMessage("Aton driver is not loaded", 2)
            return

        # Try to Stop if it's already running
        c4d.CallCommand(ARNOLD_RENDER_COMMAND, 2)

        # Read settings
        settings = c4d.BaseContainer()
        settings.SetLink(C4DTOA_RENDEROVERRIDE_DRIVER, self.driver)
        settings.SetBool(C4DTOA_RENDEROVERRIDE_USE_REGION, True)
        settings.SetBool(C4DTOA_RENDEROVERRIDE_ONLY_BEAUTY, False)
        settings.SetBool(C4DTOA_RENDEROVERRIDE_EXPORT_ALL_DRIVERS, False)
        settings.SetBool(C4DTOA_RENDEROVERRIDE_PROGRESSIVE_REFINEMENT, True)

        # Sequence Rendering
        if self.GetBool(self.SequenceOnID):
            start = self.GetInt32(self.SequenceStartID)
            end = self.GetInt32(self.SequenceEndID)
            step = self.GetInt32(self.SequenceStepID)
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_FRAME_START, start)
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_FRAME_END, end)
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_FRAME_STEP, step)
            settings.SetBool(C4DTOA_RENDEROVERRIDE_PROGRESSIVE_REFINEMENT, False)

        # Store settings
        doc.GetSettingsInstance(c4d.DOCUMENTSETTINGS_DOCUMENT).SetContainer(ARNOLD_RENDER_OVERRIDES, settings)

        # Set Overrids
        self.updateIPR()

        # Start IPR or Sequnce Rendering
        if not self.GetBool(self.SequenceOnID):
            self.logMessage("IPR started")
            c4d.CallCommand(ARNOLD_RENDER_COMMAND, 1)
        else:
            self.logMessage("Sequence render started")
            c4d.CallCommand(ARNOLD_RENDER_COMMAND, 5)
            self.logMessage("Sequence render finished")

    def updateIPR(self, attr=None):
        '''Updates IPR Changes'''
        # Get current settings
        doc = documents.GetActiveDocument()
        settings = doc.GetSettingsInstance(c4d.DOCUMENTSETTINGS_DOCUMENT).GetContainer(ARNOLD_RENDER_OVERRIDES)

        # Camera Update
        if attr == None or attr == 0:
            selCamIndex = self.GetInt32(self.CameraID)
            if selCamIndex > 0:
                camera = self.cameras[selCamIndex - 1]
                settings.SetLink(C4DTOA_RENDEROVERRIDE_CAMERA, camera)
            else:
                settings.RemoveData(C4DTOA_RENDEROVERRIDE_CAMERA)

        # Resolution and Region Update
        if attr == None or attr == 1:
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_XRES, self.getRegion(0))
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_YRES, self.getRegion(1))
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_REGION_X1, self.getRegion(2))
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_REGION_Y1, self.getRegion(3))
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_REGION_X2, self.getRegion(4))
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_REGION_Y2, self.getRegion(5))

        # Camera AA Update
        if attr == None or attr == 2:
            CameraAA = self.GetInt32(self.CameraAAID)
            settings.SetInt32(C4DTOA_RENDEROVERRIDE_AA_SAMPLES, CameraAA)

        # Ignore options Update
        if attr == None or attr == 3:
            settings.SetBool(C4DTOA_RENDEROVERRIDE_IGNORE_MOTION_BLUR, self.GetBool(self.MotionBlurID))
            settings.SetBool(C4DTOA_RENDEROVERRIDE_IGNORE_SUBDIV, self.GetBool(self.SubdivID))
            settings.SetBool(C4DTOA_RENDEROVERRIDE_IGNORE_DISPLACEMENT, self.GetBool(self.DisplaceID))
            settings.SetBool(C4DTOA_RENDEROVERRIDE_IGNORE_BUMP, self.GetBool(self.BumpID))
            settings.SetBool(C4DTOA_RENDEROVERRIDE_IGNORE_SSS, self.GetBool(self.SSSID))

        # Store the new settings
        doc.GetSettingsInstance(c4d.DOCUMENTSETTINGS_DOCUMENT).SetContainer(ARNOLD_RENDER_OVERRIDES, settings)

        # Trigger update
        c4d.CallCommand(ARNOLD_RENDER_COMMAND, 4)

    def stopIPR(self):
        '''Stop Rendering'''
        self.logMessage("Render stopped")
        c4d.CallCommand(ARNOLD_RENDER_COMMAND, 2)

class Aton_Main(plugins.CommandData):

    dialog = None

    def Execute (self, doc):
        if self.dialog is None:
            self.dialog = Aton()
        return self.dialog.Open(dlgtype = c4d.DLG_TYPE_ASYNC, pluginid = PLUGIN_ID)

    def RestoreLayout (self, sec_ref):
        if self.dialog is None:
            self.dialog = Aton()
        return self.dialog.Restore(pluginid=PLUGIN_ID, secret=sec_ref)

if __name__ == "__main__":
    path, fn = os.path.split(__file__)
    bmp = bitmaps.BaseBitmap()
    bmp.InitWith(os.path.join(path, "aton_icon.png"))
    plugins.RegisterCommandPlugin(PLUGIN_ID, "Aton", 0, bmp, "", Aton_Main())
