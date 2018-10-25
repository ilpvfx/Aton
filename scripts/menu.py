import nuke
toolbar = nuke.menu("Nodes")
mainToolBar=nuke.toolbar("Nodes")
m = mainToolBar.addMenu("Image")
m.addCommand("Aton", "nuke.createNode(\"Aton\")")
