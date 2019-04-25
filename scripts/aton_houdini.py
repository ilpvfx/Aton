import os
import re
import psutil

import hou

from hutil.Qt import QtCore, QtWidgets, QtGui
from htoa.node.parms import HaNodeSetStr
from htoa.node.node import nodeSetArrayString

from arnold import *

__author__ = "Ryan Heniser, Vahan Sosoyan"
__copyright__ = "2019 All rights reserved. See Copyright.txt for more details."
__version__ = "1.3.1"


def warn(msg, *params):
    """ Warn message in Arnold Rendering process
    """
    header = "[%s] " % __name__
    AiMsgWarning(header + msg, *params)


def atonPatch():
    """ Patching HtoA to override the driver
    """
    import htoa.object.rop
    import htoa.session
    # Only monkey patch once -- the arnold.py soho script from HtoA can and
    # typically will be called many times. Only monkey patch (decorate) the
    # generate method once.
    if htoa.object.rop.HaRop.generate.__name__ == "generate":
        htoa.session.HaRop.generate = generate_decorated(htoa.session.HaRop.generate)


def aton_update(self):
    """ Runing this function for Aton overrides
    """
    if self.session.isInteractiveRender():

        options_node = AiUniverseGetOptions()

        if AiNodeLookUpUserParameter(options_node, "aton_enable"):
            
            if AiNodeGetBool(options_node, "aton_enable"):
                
                driver = "driver_aton"
                
                if not (AiNodeEntryLookUp(driver) is None):
                    
                    aton_node = get_aton_driver(self, driver, "aton")

                    AiNodeSetStr(aton_node, "host", AiNodeGetStr(options_node, "aton_host"))
                    AiNodeSetInt(aton_node, "port", AiNodeGetInt(options_node, "aton_port"))
                    AiNodeSetStr(aton_node, "output", AiNodeGetStr(options_node, "aton_output"))
                    
                    if AiNodeLookUpUserParameter(options_node, "aton_bucket"):
                        AiNodeSetStr(options_node, "bucket_scanning",
                                     AiNodeGetStr(options_node, "aton_bucket"))

                    if AiNodeLookUpUserParameter(options_node, "aton_region_min_x"):
                        AiNodeSetInt(options_node, "region_min_x",
                                     AiNodeGetInt(options_node, "aton_region_min_x"))

                    if AiNodeLookUpUserParameter(options_node, "aton_region_min_y"):
                        AiNodeSetInt(options_node, "region_min_y",
                                     AiNodeGetInt(options_node, "aton_region_min_y"))

                    if AiNodeLookUpUserParameter(options_node, "aton_region_max_x"):
                        AiNodeSetInt(options_node, "region_max_x",
                                     AiNodeGetInt(options_node, "aton_region_max_x"))

                    if AiNodeLookUpUserParameter(options_node, "aton_region_max_y"):
                        AiNodeSetInt(options_node, "region_max_y",
                                     AiNodeGetInt(options_node, "aton_region_max_y"))
                    
                    if AiNodeLookUpUserParameter(options_node, "aton_ignore_mbl"):
                        AiNodeSetBool(options_node, "ignore_motion_blur",
                                      AiNodeGetBool(options_node, "aton_ignore_mbl"))

                    if AiNodeLookUpUserParameter(options_node, "aton_ignore_sdv"):
                        AiNodeSetBool(options_node, "ignore_subdivision",
                                      AiNodeGetBool(options_node, "aton_ignore_sdv"))

                    if AiNodeLookUpUserParameter(options_node, "aton_ignore_dsp"):
                        AiNodeSetBool(options_node, "ignore_displacement",
                                      AiNodeGetBool(options_node, "aton_ignore_dsp"))

                    if AiNodeLookUpUserParameter(options_node, "aton_ignore_bmp"):
                        AiNodeSetBool(options_node, "ignore_bump",
                                      AiNodeGetBool(options_node, "aton_ignore_bmp"))

                    if AiNodeLookUpUserParameter(options_node, "aton_ignore_sss"):
                        AiNodeSetBool(options_node, "ignore_sss",
                                      AiNodeGetBool(options_node, "aton_ignore_sss"))

                    # Get the outputs string array param (on the options node) as a python list
                    array = AiNodeGetArray(options_node, "outputs")
                    elements = AiArrayGetNumElements(array)
                    outputs = [AiArrayGetStr(array, i) for i in xrange(elements)]

                    # RGBA primary should be the first in outputs--its last string
                    # should be the node name of the driver_houdini (IPR) main driver
                    name = outputs[0].split()[-1]

                    # Ignoring variance outputs coming from Noice
                    aton_outputs = \
                        [i.replace(name, AiNodeGetName(aton_node)) for i in outputs if "variance_filter" not in i]
                    nodeSetArrayString(options_node, "outputs", aton_outputs)
                
                else:
                    warn("Aton Driver was not found.")
            else:
                warn("Aton is not Enabled.")
        else:
            warn("Aton User Options was not found.")


def generate_decorated(func):
    """ Decorating a generate method
    """
    def generate_decorator(self, *args, **kwargs):
        result = func(self, *args, **kwargs)
        aton_update(self)
        return result
    
    return generate_decorator


def get_aton_driver(self, node_entry_name, new_sub_str):
    """ Get Aton Driver Arnold Node
    """
    from htoa.object.camera import cameraTag

    node_iter = AiUniverseGetNodeIterator(AI_NODE_DRIVER)

    while not AiNodeIteratorFinished(node_iter):
        node = AiNodeIteratorGetNext(node_iter)
        node_entry = AiNodeGetNodeEntry(node)
        if node_entry_name == AiNodeEntryGetName(node_entry):
            return node

    driver_aton_node = AiNode(node_entry_name)
    cam_tag = cameraTag(self.session.camera_name)
    HaNodeSetStr(driver_aton_node, "name",
                 (self.path + ":" + new_sub_str + (":%s" % cam_tag if cam_tag else "")))
    return driver_aton_node


def get_host():
    """ Returns a host name from Aton driver
    """
    aton_host = os.getenv("ATON_HOST")
    
    if aton_host is None:
        return "127.0.0.1"
    else:
        return str(aton_host)


def get_port():
    """ Returns a port number from Aton driver
    """
    aton_port = os.getenv("ATON_PORT")
    
    if aton_port is None:
        return 9201
    else:
        return int(aton_port)


def get_ui_instance_count(name):
    """ Get UI instance count for setting a Port number
    """
    widgets = QtWidgets.QApplication.instance().topLevelWidgets()
    instances = [w.instance for w in widgets if w.objectName() == name]
    
    res = 0
    while True:
        if res in instances:
            res += 1
        else:
            return res


def get_output_drivers(path=False):
    """ Returns a list of all output driver names
    """
    rops = hou.nodeType(hou.nodeTypeCategories()["Driver"], "arnold").instances()
    
    if path:
        return [i.path() for i in rops]
    
    return rops


def get_bucket_modes():
    """ Get the list of Bucket Scanning modes
    """
    rops = get_output_drivers()
    if rops:
        parm_template_group = rops[0].parmTemplateGroup()
        parm_tamplate_name = "ar_bucket_scanning"
        parm_template_exist = parm_template_group.find(parm_tamplate_name)
        if parm_template_exist:
            return rops[0].parm(parm_tamplate_name).parmTemplate().menuItems()


def get_all_cameras(path=False):
    """ Returns a list of all camera names
    """
    cameras = hou.nodeType(hou.nodeTypeCategories()["Object"], "cam").instances()
    cameras += hou.nodeType(hou.nodeTypeCategories()["Object"], "stereocam").instances()
    
    if path:
        return [i.path() for i in cameras]
    
    return cameras


class HickStatus(QtCore.QThread):
    """ Checks whether hick process is running
        and emits signal when it's finished
    """
    finished = QtCore.Signal(bool)

    def __init__(self, ipr):
        super(HickStatus, self).__init__()

        self._ipr = ipr

    def run(self):
        while self._ipr.isActive():
            if self.is_finished():
                self.finished.emit(True)

    def is_finished(self):
        for p in psutil.Process(os.getpid()).children(recursive=True):
            if p.name().startswith('hick'):
                try:
                    return (p.cpu_percent(interval=1) < 1.0)
                except psutil.NoSuchProcess:
                    return


class Output(object):
    """ Output object holds ROP attributes
    """
    def __init__(self, rop=None):

        self.rop = None
        self.name = None
        self.path = None
        self.camera = None
        self.override_camera_res = None
        self.res_fraction = None
        self.resolution = (0, 0)
        self.aa_samples = 0
        self.bucket_scanning = None
        self.pixel_aspect = 0
        self.user_options_enable = None
        self.user_options_parm = None
        self.user_options = None

        if rop:
            user_options_parm_name = "ar_user_options"
            parm_template_group = rop.parmTemplateGroup()
            parm_template = parm_template_group.find(user_options_parm_name)

            if parm_template:
                self._init_attributes(rop)
    
    @property
    def camera_path(self):
        """ Returns camera path from current camera objcet
        """
        if self.camera:
            return self.camera.path()

    def _init_attributes(self, rop):
        """ Inilialize ROP Attributes
        """
        self.rop = rop
        self.name = rop.name()
        self.path = rop.path()

        self.camera = self._get_camera()
        self._camera_resolution = self.camera.parmTuple("res").eval() if self.camera else (0, 0)
        self._camera_pixel_aspect = self.camera.parm("aspect").eval() if self.camera else 1.0

        self.override_camera_res = rop.parm("override_camerares").eval()
        self.res_fraction = rop.parm("res_fraction").eval()
        self.res_override = rop.parmTuple("res_override").eval()
        self.resolution = self._get_resolution()
        self.pixel_aspect = self._get_pixel_aspect()
        self.aa_samples = rop.parm("ar_AA_samples").eval()
        self.bucket_scanning = rop.parm("ar_bucket_scanning").eval()
        self.user_options_enable = rop.parm("ar_user_options_enable").eval()
        self.user_options_parm = rop.parm("ar_user_options")
        self.user_options = self.user_options_parm.eval()
   
    def _get_camera(self):
        """ Get Camera object
        """
        camera = hou.node(self.rop.parm("camera").eval())
        if camera is None:
            scene_cameras = get_all_cameras()
            if scene_cameras:
                camera = scene_cameras[0]
        return camera

    def _get_resolution(self):
        """ Get Resolution touple
        """
        if self.rop.parm("override_camerares").eval():
            res_scale = self.rop.parm("res_fraction").eval()
            
            if res_scale == "specific":
                return self.rop.parmTuple("res_override").eval()
            else:
                return (int(self._camera_resolution[0] * float(res_scale)),
                        int(self._camera_resolution[1] * float(res_scale)))

        return self._camera_resolution

    def _get_pixel_aspect(self):
        """ Get Camera Pixel Aspect Ration
        """
        if self.rop.parm("override_camerares").eval():
            return self.rop.parm("aspect_override").eval()
        else:
            return self._camera_pixel_aspect

    def rollback_camera(self):
        """ Rollback ROP camera to default
        """
        try:
            self.rop.parm("camera").set(self.camera_path)
        except hou.ObjectWasDeleted:
            pass
    
    def rollback_resolution(self):
        """ Rollback Resolution to default
        """
        try:
            self.rop.parm("override_camerares").set(self.override_camera_res)
            self.rop.parm("res_fraction").set(self.res_fraction)
            self.rop.parmTuple("res_override").set(self.res_override)
            self.rop.parm("aspect_override").set(self.pixel_aspect)
        except hou.ObjectWasDeleted:
            pass
    
    def rollback_aa_samples(self):
        """ Rollback AA Samples to default
        """
        try:
            self.rop.parm("ar_AA_samples").set(self.aa_samples)
        except hou.ObjectWasDeleted:
            pass
    
    def rollback_user_options(self):
        """ Rollback User Options to default
        """
        self.user_options = re.sub("declare aton_enable.*", "", self.user_options)
        try:
            self.rop.parm("ar_user_options_enable").set(self.user_options_enable)
            self.rop.parm("ar_user_options").set(self.user_options)
        except hou.ObjectWasDeleted:
            pass


class BoxWidget(QtWidgets.QFrame):
    """ Abstract Class for UI Widgets
    """
    def __init__(self, label, first=True):
        super(BoxWidget, self).__init__()

        self.layout = QtWidgets.QHBoxLayout(self)
        self.layout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
        self.layout.setContentsMargins(0, 0, 0, 0)

        self.label = QtWidgets.QLabel(label)

        if first:
            self.label.setText(label + ":")
            self.label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignCenter)
            self.label.setMinimumSize(75, 20)
            self.label.setMaximumSize(75, 20)

        self.layout.addWidget(self.label)


class LineEditBox(BoxWidget):
    """ QLineEdit Implementation
    """
    def __init__(self, label, text="", first=True):
        super(LineEditBox, self).__init__(label, first)

        self.lineEditBox = QtWidgets.QLineEdit()
        self.lineEditBox.setText(text)
        self.layout.addWidget(self.lineEditBox)

    def set_enabled(self, value):
        self.label.setEnabled(value)
        self.lineEditBox.setEnabled(value)

    def text(self):
        return self.lineEditBox.text()

    def set_text(self, text):
        self.lineEditBox.setText(text)


class SliderBox(BoxWidget):
    """ SliderBox Widget based on QSpinbox and QSlider
    """
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

    def set_minimum(self, spin_value=None, slider_value=None):
        if spin_value is not None:
            self.spinBox.setMinimum(spin_value)
        if slider_value is not None:
            self.slider.setMinimum(slider_value)

    def set_maximum(self, spin_value=None, slider_value=None):
        if spin_value is not None:
            self.spinBox.setMaximum(spin_value)
        if slider_value is not None:
            self.slider.setMaximum(slider_value)

    def set_value(self, spin_value=None, slider_value=None):
        if slider_value is not None:
            self.slider.setValue(slider_value)
        if spin_value is not None:
            self.spinBox.setValue(spin_value)

    def value(self):
        return self.spinBox.value()

    def connect(self, func):
        self.slider.valueChanged.connect(func)

    def set_enabled(self, value):
        self.label.setEnabled(value)
        self.spinBox.setEnabled(value)
        self.slider.setEnabled(value)

    @property
    def value_changed(self):
        return self.spinBox.valueChanged


class SpinBox(BoxWidget):
    """ QSpinBox  implementation
    """
    def __init__(self, label, value=0, first=True):
        super(SpinBox, self).__init__(label, first)
        self.spin_box = QtWidgets.QSpinBox()
        self.spin_box.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.spin_box.setRange(-99999, 99999)
        self.spin_box.setMaximumSize(50, 20)
        self.spin_box.setValue(value)

        self.layout.addWidget(self.spin_box)

    def value(self):
        return self.spin_box.value()

    def set_value(self, value):
        self.spin_box.setValue(value)

    @property
    def value_changed(self):
        return self.spin_box.valueChanged


class ComboBox(BoxWidget):
    """ QComboBox implemenation
    """
    def __init__(self, label, first=True):
        super(ComboBox, self).__init__(label, first)
        self.items = list()

        self.combo_box = QtWidgets.QComboBox()
        self.combo_box.setSizePolicy(QtWidgets.QSizePolicy.Expanding,
                                     QtWidgets.QSizePolicy.Fixed)
        self.current_index_changed = self.combo_box.currentIndexChanged

        self.layout.addWidget(self.combo_box)

    def set_enabled(self, value):
        self.label.setEnabled(value)
        self.combo_box.setEnabled(value)

    def set_current_index(self, value):
        self.combo_box.setCurrentIndex(value)

    def set_current_name(self, value):
        for idx, item in enumerate(self.items):
            if item == value:
                self.combo_box.setCurrentIndex(idx)

    def current_index(self):
        return self.combo_box.currentIndex()

    def current_name(self):
        index = self.combo_box.currentIndex()
        if self.items:
            return self.items[index]

    def add_items(self, items):
        if items:
            for i in items:
                self.combo_box.addItem(i)
            self.items += items

    def new_items(self, items):
        self.clear()
        if items:
            for i in items:
                self.combo_box.addItem(i)
            self.items += items

    def clear(self):
        self.combo_box.clear()
        self.items = []


class CheckBox(BoxWidget):
    """ QCheckBox implemenation
    """
    def __init__(self, label, title="", first=True):
        super(CheckBox, self).__init__(label, first)
        self.check_box = QtWidgets.QCheckBox(title)

        self.layout.addWidget(self.check_box)

    @property
    def state_changed(self):
        return self.check_box.stateChanged

    @property
    def toggled(self):
        return self.check_box.toggled

    def is_checked(self):
        return self.check_box.isChecked()

    def set_checked(self, value):
        self.check_box.setChecked(value)

    def set_enabled(self, value):
        self.check_box.setEnabled(value)


class Aton(QtWidgets.QWidget):
    """ Main Aton UI Object
    """
    def __init__(self, icon_path=None):
        QtWidgets.QWidget.__init__(self)
        
        self.obj_name = self.__class__.__name__.lower()

        # Properties
        self._output = None
        self._hick_status = None
        
        # Default Settings
        self.default_port = get_port()
        self.default_host = get_host()
        self.instance = get_ui_instance_count(self.obj_name)
        self.outputs_list = [Output(rop) for rop in get_output_drivers()]

        # Init UI
        self.setObjectName(self.obj_name)
        self.setProperty("saveWindowPref", True)
        self.setProperty("houdiniStyle", True)
        self.setStyleSheet(hou.qt.styleSheet())
        self.setWindowIcon(QtGui.QIcon(icon_path))

        # Setup UI
        self.host_line_edit = None
        self.host_check_box = None
        self.port_slider = None
        self.port_check_box = None
        self.output_combo_box = None
        self.camera_combo_box = None
        self.ipr_update_check_box = None
        self.bucket_combo_box = None
        self.resolution_combo_box = None
        self.camera_aa_slider = None
        self.render_region_x_spin_box = None
        self.render_region_y_spin_box = None
        self.render_region_r_spin_box = None
        self.render_region_t_spin_box = None
        self.overscan_slider = None
        self.sequence_checkbox = None
        self.seq_start_spin_box = None
        self.seq_end_spin_box = None
        self.seq_step_spin_box = None
        self.seq_rebuild_checkbox = None
        self.motion_blur_check_box = None
        self.subdivs_check_box = None
        self.displace_check_box = None
        self.bump_check_box = None
        self.sss_check_box = None

        # Build UI
        self.build_ui()

        # Adding a reset_callback
        hou.hipFile.addEventCallback(self.reset_callback)

    @property
    def ipr(self):
        """ Returns IPRViewer object
        """
        desk = hou.ui.curDesktop()
        return desk.paneTabOfType(hou.paneTabType.IPRViewer)

    @property
    def output(self):
        """ Returns output object based on the current Aton UI selection
        """
        idx = self.output_combo_box.current_index()
        if 0 <= idx < len(self.outputs_list):
            self._output = self.outputs_list[idx]
        else:
            self._output = Output()
        return self._output

    @property
    def hick_status(self):
        """ Returns HickStatus object
        """
        if self._hick_status is None:
            self._hick_status = HickStatus(self.ipr)
            self._hick_status.finished.connect(self.change_time)

        return self._hick_status

    @property
    def port(self):
        """ Returns Port number based on AtonU UI instance count
        """
        if self.instance > 0:
            return self.default_port + self.instance
        else:
            return self.default_port

    def reset_callback(self, event):
        """ Reset the UI if the scene was cleared or the new scene was loaded
        """
        if event == hou.hipFileEventType.AfterLoad or \
           event == hou.hipFileEventType.AfterClear:
            self.reset_ui()

    def closeEvent(self, event):
        """ Called when the UI has been closed
        """
        if self.ipr.isActive():
            self.ipr.killRender()
        self.remove_aton_overrides()
        
        self.setParent(None)
        self.deleteLater()
        self.destroy()

        hou.hipFile.removeEventCallback(self.reset_callback)

    def generate_res_list(self):
        """ Generate Resolution List for the Aton UI
        """
        xres, yres = self.output.resolution[0], self.output.resolution[1]
        return ["%d%% (%dx%d)" % (i, xres/100.0*i, yres/100.0*i) for i in [100.0, 75.0, 50.0, 25.0, 10.0, 5.0]]

    def build_ui(self):
        """ Build Aton UI
        """

        # Set UI Flags
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setAttribute(QtCore.Qt.WA_AlwaysShowToolTips)

        # Main Layout
        main_layout = QtWidgets.QVBoxLayout()

        # General Group
        general_group_box = QtWidgets.QGroupBox("General")
        general_layout = QtWidgets.QVBoxLayout(general_group_box)

        # Host Layout
        host_layout = QtWidgets.QHBoxLayout()
        self.host_line_edit = LineEditBox("Host", self.default_host)
        self.host_check_box = CheckBox("", "", False)
        self.host_check_box.set_checked(True)
        self.host_check_box.state_changed.connect(self.host_line_edit.set_enabled)
        host_layout.addWidget(self.host_line_edit)
        host_layout.addWidget(self.host_check_box)

        # Port Layout
        port_layout = QtWidgets.QHBoxLayout()
        self.port_slider = SliderBox("Port")
        self.port_slider.set_minimum(0, 0)
        self.port_slider.set_maximum(9999, 15)
        self.port_slider.set_value(self.port, self.port - self.default_port)
        self.port_slider.connect(self.port_update_ui)
        self.port_check_box = CheckBox("", "", False)
        self.port_check_box.set_checked(True)
        self.port_check_box.state_changed.connect(self.port_slider.set_enabled)
        port_layout.addWidget(self.port_slider)
        port_layout.addWidget(self.port_check_box)

        # Output Driver Layout
        output_driver_layout = QtWidgets.QHBoxLayout()
        self.output_combo_box = ComboBox("Output")
        self.output_combo_box.add_items([i.path for i in self.outputs_list])
        self.output_combo_box.current_index_changed.connect(self.output_update_ui)
        self.setWindowTitle("%s - %s" % (self.__class__.__name__, self.output.name))
        output_driver_layout.addWidget(self.output_combo_box)

        # Camera Layout
        camera_layout = QtWidgets.QHBoxLayout()
        self.camera_combo_box = ComboBox("Camera")
        self.camera_combo_box.add_items(get_all_cameras(path=True))
        self.camera_combo_box.set_current_name(self.output.camera_path)
        camera_layout.addWidget(self.camera_combo_box)

        # Overrides Group
        overrides_group_box = QtWidgets.QGroupBox("Overrides")
        overrides_layout = QtWidgets.QVBoxLayout(overrides_group_box)

        # IPR Update Layout
        ipr_update_layout = QtWidgets.QHBoxLayout()
        self.ipr_update_check_box = CheckBox("IPR", "Auto Update")
        self.ipr_update_check_box.set_checked(self.ipr.isAutoUpdateOn())
        self.ipr_update_check_box.state_changed.connect(lambda: self.ipr.setAutoUpdate(
            self.ipr_update_check_box.is_checked()))
        ipr_update_layout.addWidget(self.ipr_update_check_box)

        # Bucket Layout
        bucket_layout = QtWidgets.QHBoxLayout()
        self.bucket_combo_box = ComboBox("Bucket Scan")
        self.bucket_combo_box.add_items(get_bucket_modes())
        self.bucket_combo_box.set_current_name(self.output.bucket_scanning)
        bucket_layout.addWidget(self.bucket_combo_box)

        # Resolution Layout
        resolution_layout = QtWidgets.QHBoxLayout()
        self.resolution_combo_box = ComboBox("Resolution")
        self.resolution_combo_box.add_items(self.generate_res_list())
        resolution_layout.addWidget(self.resolution_combo_box)

        # Camera AA Layout
        camera_aa_layout = QtWidgets.QHBoxLayout()
        self.camera_aa_slider = SliderBox("Camera (AA)")
        self.camera_aa_slider.set_minimum(-64, -3)
        self.camera_aa_slider.set_maximum(64, 16)
        self.camera_aa_slider.set_value(self.output.aa_samples, self.output.aa_samples)
        camera_aa_layout.addWidget(self.camera_aa_slider)

        # Render region layout
        render_region_layout = QtWidgets.QHBoxLayout()
        self.render_region_x_spin_box = SpinBox("Region X")
        self.render_region_y_spin_box = SpinBox("Y:", 0, False)
        self.render_region_r_spin_box = SpinBox("R:", 0, False)
        self.render_region_t_spin_box = SpinBox("T:", 0, False)
        self.render_region_r_spin_box.set_value(self.output.resolution[0])
        self.render_region_t_spin_box.set_value(self.output.resolution[1])
        render_region_reset_button = QtWidgets.QPushButton("Reset")
        render_region_get_nuke_button = QtWidgets.QPushButton("Get")
        render_region_reset_button.clicked.connect(self.reset_region_ui)
        render_region_get_nuke_button.clicked.connect(self.get_nuke_crop_node)
        render_region_layout.addWidget(self.render_region_x_spin_box)
        render_region_layout.addWidget(self.render_region_y_spin_box)
        render_region_layout.addWidget(self.render_region_r_spin_box)
        render_region_layout.addWidget(self.render_region_t_spin_box)
        render_region_layout.addWidget(render_region_reset_button)
        render_region_layout.addWidget(render_region_get_nuke_button)

        # Overscan Layout
        overscan_layout = QtWidgets.QHBoxLayout()
        self.overscan_slider = SliderBox("Overscan")
        self.overscan_slider.set_minimum(0)
        self.overscan_slider.set_maximum(9999, 250)
        self.overscan_slider.set_value(0, 0)
        overscan_layout.addWidget(self.overscan_slider)

        # Ignore Group
        ignores_group_box = QtWidgets.QGroupBox("Ignore")
        ignores_group_box.setMaximumSize(9999, 75)

        # Ignore Layout
        ignores_layout = QtWidgets.QVBoxLayout(ignores_group_box)
        ignore_layout = QtWidgets.QHBoxLayout()
        self.motion_blur_check_box = CheckBox("", "Motion Blur", False)
        self.subdivs_check_box = CheckBox("", "Subdivs", False)
        self.displace_check_box = CheckBox("", "Displace", False)
        self.bump_check_box = CheckBox("", "Bump", False)
        self.sss_check_box = CheckBox("", "SSS", False)
        ignore_layout.addWidget(self.motion_blur_check_box)
        ignore_layout.addWidget(self.subdivs_check_box)
        ignore_layout.addWidget(self.displace_check_box)
        ignore_layout.addWidget(self.bump_check_box)
        ignore_layout.addWidget(self.sss_check_box)

        # Sequence layout
        sequence_group_box = QtWidgets.QGroupBox("Sequence")
        sequence_layout = QtWidgets.QHBoxLayout(sequence_group_box)
        self.sequence_checkbox = CheckBox("Enable")
        self.seq_start_spin_box = SpinBox("Start:", hou.playbar.frameRange()[0], False)
        self.seq_end_spin_box = SpinBox("End:", hou.playbar.frameRange()[1], False)
        self.seq_step_spin_box = SpinBox("Step:", 1, False)
        self.seq_rebuild_checkbox = CheckBox("", "Rebuild", False)
        self.seq_rebuild_checkbox.setEnabled(False)
        self.seq_start_spin_box.setEnabled(False)
        self.seq_end_spin_box.setEnabled(False)
        self.seq_step_spin_box.setEnabled(False)
        self.sequence_checkbox.toggled.connect(self.seq_start_spin_box.setEnabled)
        self.sequence_checkbox.toggled.connect(self.seq_end_spin_box.setEnabled)
        self.sequence_checkbox.toggled.connect(self.seq_step_spin_box.setEnabled)
        self.sequence_checkbox.toggled.connect(self.seq_rebuild_checkbox.setEnabled)
        sequence_layout.addWidget(self.sequence_checkbox)
        sequence_layout.addWidget(self.seq_start_spin_box)
        sequence_layout.addWidget(self.seq_end_spin_box)
        sequence_layout.addWidget(self.seq_step_spin_box)
        sequence_layout.addWidget(self.seq_rebuild_checkbox)

        # Main Buttons Layout
        main_buttons_layout = QtWidgets.QHBoxLayout()
        start_button = QtWidgets.QPushButton("Start / Refresh")
        stop_button = QtWidgets.QPushButton("Stop")
        reset_button = QtWidgets.QPushButton("Reset")
        start_button.clicked.connect(self.start_render)
        stop_button.clicked.connect(self.stop_render)
        reset_button.clicked.connect(self.reset_ui)
        main_buttons_layout.addWidget(start_button)
        main_buttons_layout.addWidget(stop_button)
        main_buttons_layout.addWidget(reset_button)

        # Add Layouts to Main
        general_layout.addLayout(host_layout)
        general_layout.addLayout(port_layout)
        general_layout.addLayout(output_driver_layout)
        overrides_layout.addLayout(ipr_update_layout)
        overrides_layout.addLayout(camera_layout)
        overrides_layout.addLayout(bucket_layout)
        overrides_layout.addLayout(resolution_layout)
        overrides_layout.addLayout(camera_aa_layout)
        overrides_layout.addLayout(render_region_layout)
        overrides_layout.addLayout(overscan_layout)
        ignores_layout.addLayout(ignore_layout)

        main_layout.addWidget(general_group_box)
        main_layout.addWidget(overrides_group_box)
        main_layout.addWidget(sequence_group_box)
        main_layout.addWidget(ignores_group_box)
        main_layout.addLayout(main_buttons_layout)

        self.setLayout(main_layout)

        self.camera_combo_box.current_index_changed.connect(self.add_aton_overrides)
        self.bucket_combo_box.current_index_changed.connect(self.add_aton_overrides)
        self.resolution_combo_box.current_index_changed.connect(self.add_aton_overrides)
        self.camera_aa_slider.value_changed.connect(self.add_aton_overrides)
        self.render_region_x_spin_box.value_changed.connect(self.add_aton_overrides)
        self.render_region_y_spin_box.value_changed.connect(self.add_aton_overrides)
        self.render_region_r_spin_box.value_changed.connect(self.add_aton_overrides)
        self.render_region_t_spin_box.value_changed.connect(self.add_aton_overrides)
        self.overscan_slider.value_changed.connect(self.add_aton_overrides)
        self.motion_blur_check_box.toggled.connect(self.add_aton_overrides)
        self.subdivs_check_box.toggled.connect(self.add_aton_overrides)
        self.displace_check_box.toggled.connect(self.add_aton_overrides)
        self.bump_check_box.toggled.connect(self.add_aton_overrides)
        self.sss_check_box.toggled.connect(self.add_aton_overrides)

    def port_update_ui(self, value):
        """ Update Port UI
        """
        self.port_slider.spinBox.setValue(value + self.default_port)

    def output_update_ui(self, index):
        """ Update the UI when changing the output rop
        """
        if index >= 0:
            self.res_update_ui()
            self.camera_combo_box.set_current_name(self.output.camera_path)
            self.bucket_combo_box.set_current_name(self.output.bucket_scanning)
            self.camera_aa_slider.set_value(self.output.aa_samples, self.output.aa_samples)
            self.render_region_r_spin_box.set_value(self.output.resolution[0])
            self.render_region_t_spin_box.set_value(self.output.resolution[1])

        self.setWindowTitle("%s - %s" % (self.__class__.__name__, self.output.name))

    def res_update_ui(self):
        """ Update Resolution UI
        """
        index = self.resolution_combo_box.current_index()
        self.resolution_combo_box.new_items(self.generate_res_list())
        self.resolution_combo_box.set_current_index(index)

    def reset_ui(self):
        """ Reset UI
        """
        if self.ipr.isActive():
            self.stop_render()

        # Update Defualt Settings
        self.outputs_list = [Output(rop) for rop in get_output_drivers()]

        current_output_name = self.output_combo_box.current_name()

        self.host_check_box.set_checked(True)
        self.port_check_box.set_checked(True)
        self.host_line_edit.set_text(self.default_host)
        self.port_slider.set_value(self.port, self.port - self.default_port)
        self.ipr_update_check_box.set_checked(True)

        self.bucket_combo_box.new_items(get_bucket_modes())
        self.camera_combo_box.new_items(get_all_cameras(path=True))
        self.resolution_combo_box.new_items(self.generate_res_list())
        self.output_combo_box.new_items(get_output_drivers(path=True))
        self.output_combo_box.set_current_name(current_output_name)

        self.render_region_x_spin_box.set_value(0)
        self.render_region_y_spin_box.set_value(0)
        self.overscan_slider.set_value(0, 0)
        self.motion_blur_check_box.set_checked(False)
        self.subdivs_check_box.set_checked(False)
        self.displace_check_box.set_checked(False)
        self.bump_check_box.set_checked(False)
        self.sss_check_box.set_checked(False)

        self.output_update_ui(self.output_combo_box.current_index())

    def reset_region_ui(self):
        """ Reset Region UI
        """
        self.render_region_x_spin_box.set_value(0)
        self.render_region_y_spin_box.set_value(0)
        self.render_region_r_spin_box.set_value(self.output.resolution[0])
        self.render_region_t_spin_box.set_value(self.output.resolution[1])
        self.overscan_slider.set_value(0, 0)

    def general_ui_set_enabled(self, value):
        """ Toggle UI Enabled during the rendering process
        """
        if value:
            if self.host_check_box.is_checked():
                self.host_line_edit.set_enabled(value)
        else:
            self.host_line_edit.set_enabled(value)
        self.host_check_box.set_enabled(value)
        
        if value:
            if self.port_check_box.is_checked():
                self.port_slider.set_enabled(value)
        else:
            self.port_slider.set_enabled(value)
        
        self.port_check_box.set_enabled(value)
        self.output_combo_box.set_enabled(value)

    def get_nuke_crop_node(self):
        """ Get crop node data from Nuke
        """
        def find_between(s, first, last):
            try:
                start = s.index(first) + len(first)
                end = s.index(last, start)
                return s[start:end]
            except ValueError:
                return ""

        clipboard = QtWidgets.QApplication.clipboard()
        data = clipboard.text()

        crop_data = None
        check_data1 = "set cut_paste_input [stack 0]"
        check_data2 = "Crop {"

        if (check_data1 in data.split("\n", 10)[0]) and \
           (check_data2 in data.split("\n", 10)[3]):
                crop_data = find_between(data.split("\n", 10)[4], "box {", "}").split()

        if len(data.split(",")) == 5:
            crop_data = data.split(",")

        if crop_data is not None:
            nk_x = float(crop_data[0])
            nk_y = float(crop_data[1])
            nk_r = float(crop_data[2])
            nk_t = float(crop_data[3])
            nk_res = float(crop_data[4])

            region_mult = self.output.resolution[0] / nk_res

            self.render_region_x_spin_box.set_value(int(nk_x * region_mult))
            self.render_region_y_spin_box.set_value(int(nk_y * region_mult))
            self.render_region_r_spin_box.set_value(int(nk_r * region_mult))
            self.render_region_t_spin_box.set_value(int(nk_t * region_mult))

    def get_resolution(self, attr, res_scale=True):
        """ Get Resolution and Region overrides
        """
        res_value = 100

        if res_scale:
            index = self.resolution_combo_box.current_index()
            
            if index == 1:
                res_value = 75
            elif index == 2:
                res_value = 50
            elif index == 3:
                res_value = 25
            elif index == 4:
                res_value = 10
            elif index == 5:
                res_value = 5

        ovr_scn_value = self.overscan_slider.value() * res_value / 100

        xres = self.output.resolution[0] * res_value / 100
        yres = self.output.resolution[1] * res_value / 100

        result = {0: lambda: xres,
                  1: lambda: yres,
                  2: lambda: (self.render_region_x_spin_box.value() * res_value / 100) - ovr_scn_value,
                  3: lambda: yres - (self.render_region_t_spin_box.value() * res_value / 100) - ovr_scn_value,
                  4: lambda: (self.render_region_r_spin_box.value() * res_value / 100) - 1 + ovr_scn_value,
                  5: lambda: (yres - (self.render_region_y_spin_box.value() *
                                      res_value / 100)) - 1 + ovr_scn_value}[attr]()

        return result

    def start_render(self, caller=None):
        """ Start Button Command
        """
        if self.output.rop is not None:
            
            # Set IPR Options
            try:
                self.ipr.setRopNode(self.output.rop)
            except hou.ObjectWasDeleted:
                return

            self.ipr.killRender()

            # Sequence rendering mode
            if self.sequence_checkbox.is_checked():
                self.ipr.setPreview(False)
                if caller is None:
                    hou.setFrame(self.seq_start_spin_box.value())
            else:
                self.ipr.setPreview(True)

            self.ipr.startRender()
            self.ipr.pauseRender()
            
            if self.add_aton_overrides():
                self.ipr.resumeRender()
                self.general_ui_set_enabled(False)

                if self.sequence_checkbox.is_checked():
                    self.hick_status.start()
            else:
                self.stop_render()

    def change_time(self):
        """ Change time for sequence rendering
        """
        current_frame = int(hou.frame())
        end_frame = self.seq_end_spin_box.value()
        step = self.seq_step_spin_box.value()
        rebuild = self.seq_rebuild_checkbox.is_checked()

        if step > 1 and current_frame < end_frame - step + 1:
            next_frame = current_frame + step
        elif step > 1 and current_frame == end_frame - step + 1:
            next_frame = end_frame
        else:
            next_frame = current_frame + step

        if next_frame <= end_frame:
            hou.setFrame(next_frame)
        else:
            self.stop_render()
            return

        if rebuild:
            self.stop_render()
            self.start_render(self.change_time)

    def stop_render(self):
        """ Stop Button command
        """
        self.ipr.killRender()
        self.remove_aton_overrides()
        self.general_ui_set_enabled(True)

    def aa_samples_changed(self):
        """ Check if the AA Samples has been overriden
        """
        return self.camera_aa_slider.value() != self.output.aa_samples

    def camera_changed(self):
        """ Check if the Camera has been overriden
        """
        if self.output.camera is not None:
            return self.camera_combo_box.current_name() != self.output.camera.path()

    def resolution_changed(self):
        """ Check if the Resolution and Region have been overriden
        """
        x_res = self.get_resolution(0)
        y_res = self.get_resolution(1)
        x_reg = self.get_resolution(2)
        y_reg = self.get_resolution(3)
        r_reg = self.get_resolution(4)
        t_reg = self.get_resolution(5)
        
        if x_res != self.output.resolution[0] or y_res != self.output.resolution[1]:
            return True
        elif x_reg != 0 or y_reg != 0 or r_reg != x_res - 1 or t_reg != y_res - 1:
            return True
        elif self.overscan_slider.value() != 0:
            return True
        else:
            return False

    def bucket_scanning_changed(self):
        """ Check if the Bucket Scanning has been overriden
        """
        return self.bucket_combo_box.current_name() != self.output.bucket_scanning

    def ignore_mbl_changed(self):
        """ Check if the Ignore Motion Blur has been Enabled
        """
        return self.motion_blur_check_box.is_checked()

    def ignore_sdv_changed(self):
        """ Check if the Ignore Subdivisions has been Enabled
        """
        return self.subdivs_check_box.is_checked()

    def ignore_dsp_changed(self):
        """ Check if the Ignore Displacement has been Enabled
        """
        return self.displace_check_box.is_checked()

    def ignore_bmp_changed(self):
        """ Check if the Ignore Bump has been Enabled
        """
        return self.bump_check_box.is_checked()

    def ignore_sss_changed(self):
        """ Check if the Ignore Sub Surface Scattering has been Enabled
        """
        return self.sss_check_box.is_checked()

    def add_aton_overrides(self):
        """ Adds Aton overrides as a User Options
        """
        if self.ipr.isActive():

            user_options = self.output.user_options
            
            if user_options is not None:

                # Get Host and Port
                host = self.host_line_edit.text() if self.host_check_box.is_checked() else get_host()
                port = self.port_slider.value() if self.port_check_box.is_checked() else get_port()

                # Aton Attributes
                user_options += " " if user_options else ""
                user_options += "declare aton_enable constant BOOL aton_enable on "
                user_options += "declare aton_host constant STRING aton_host \"%s\" " % host
                user_options += "declare aton_port constant INT aton_port %d " % port
                user_options += "declare aton_output constant STRING aton_output \"%s\" " % self.output.name
                   
                # Enable User Options Overrides
                user_options_enabled = self.output.rop.parm("ar_user_options_enable").eval()
                if not user_options_enabled:
                    self.output.rop.parm("ar_user_options_enable").set(True)

                # Camera
                if self.camera_changed():
                    self.output.rop.parm("camera").set(self.camera_combo_box.current_name())
                else:
                    if self.output.camera is not None:
                        self.output.rop.parm("camera").set(self.output.camera.path())

                # AA Samples
                if self.aa_samples_changed():
                    self.output.rop.parm("ar_AA_samples").set(self.camera_aa_slider.value())
                else:
                    self.output.rop.parm("ar_AA_samples").set(self.output.aa_samples)

                # Resolution
                if self.resolution_changed():
                    self.output.rop.parm("override_camerares").set(True)
                    self.output.rop.parm("res_fraction").set("specific")
                    self.output.rop.parm("res_overridex").set(self.get_resolution(0))
                    self.output.rop.parm("res_overridey").set(self.get_resolution(1))
                    self.output.rop.parm("aspect_override").set(self.output.pixel_aspect)

                    # Render Region
                    user_options += "declare aton_region_min_x constant INT aton_region_min_x %d " % \
                                    self.get_resolution(2)
                    user_options += "declare aton_region_min_y constant INT aton_region_min_y %d " % \
                                    self.get_resolution(3)
                    user_options += "declare aton_region_max_x constant INT aton_region_max_x %d " % \
                                    self.get_resolution(4)
                    user_options += "declare aton_region_max_y constant INT aton_region_max_y %d " % \
                                    self.get_resolution(5)
                else:
                    self.output.rop.parm("override_camerares").set(self.output.override_camera_res)
                    self.output.rop.parm("res_fraction").set(self.output.res_fraction)
                    self.output.rop.parm("res_overridex").set(self.output.res_override[0])
                    self.output.rop.parm("res_overridey").set(self.output.res_override[1])

                    self.output.rop.parm("aspect_override").set(self.output.pixel_aspect)

                # Bucket Scanning
                if self.bucket_scanning_changed():
                    user_options += "declare aton_bucket constant STRING aton_bucket \"%s\" " % \
                                   self.bucket_combo_box.current_name()

                # Ignore Feautres
                if self.ignore_mbl_changed():
                    user_options += "declare aton_ignore_mbl constant BOOL aton_ignore_mbl %s  " % \
                                   ("on" if self.motion_blur_check_box.is_checked() else "off")
                if self.ignore_sdv_changed():
                    user_options += "declare aton_ignore_sdv constant BOOL aton_ignore_sdv %s " % \
                                   ("on" if self.subdivs_check_box.is_checked() else "off")
                if self.ignore_dsp_changed():
                    user_options += "declare aton_ignore_dsp constant BOOL aton_ignore_dsp %s " % \
                                   ("on" if self.displace_check_box.is_checked() else "off")
                if self.ignore_bmp_changed():
                    user_options += "declare aton_ignore_bmp constant BOOL aton_ignore_bmp %s " % \
                                   ("on" if self.bump_check_box.is_checked() else "off")
                if self.ignore_sss_changed():
                    user_options += "declare aton_ignore_sss constant BOOL aton_ignore_sss %s " % \
                                   ("on" if self.sss_check_box.is_checked() else "off")

                self.output.user_options_parm.set(user_options)
                
                return True

    def remove_aton_overrides(self):
        """ Remove all Aton Overrides"""
        for output in self.outputs_list:
            
            if self.camera_changed():
                output.rollback_camera()
            
            if self.resolution_changed():
                output.rollback_resolution()
            
            if self.aa_samples_changed():
                output.rollback_aa_samples()
            
            output.rollback_user_options()
