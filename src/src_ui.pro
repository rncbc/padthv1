# padthv1_ui.pro
#
NAME = padthv1

TARGET = $${NAME}_ui
TEMPLATE = lib
CONFIG += static

unix { LIBS += -L. -l$${NAME} }

include(src_ui.pri)

HEADERS = \
	config.h \
	padthv1_ui.h \
	padthv1widget.h \
	padthv1widget_env.h \
	padthv1widget_filt.h \
	padthv1widget_sample.h \
	padthv1widget_wave.h \
	padthv1widget_param.h \
	padthv1widget_keybd.h \
	padthv1widget_preset.h \
	padthv1widget_status.h \
	padthv1widget_programs.h \
	padthv1widget_controls.h \
	padthv1widget_control.h \
	padthv1widget_config.h

SOURCES = \
	padthv1_ui.cpp \
	padthv1widget.cpp \
	padthv1widget_env.cpp \
	padthv1widget_filt.cpp \
	padthv1widget_sample.cpp \
	padthv1widget_wave.cpp \
	padthv1widget_param.cpp \
	padthv1widget_keybd.cpp \
	padthv1widget_preset.cpp \
	padthv1widget_status.cpp \
	padthv1widget_programs.cpp \
	padthv1widget_controls.cpp \
	padthv1widget_control.cpp \
	padthv1widget_config.cpp

FORMS = \
	padthv1widget.ui \
	padthv1widget_control.ui \
	padthv1widget_config.ui

RESOURCES += padthv1.qrc


unix {

	OBJECTS_DIR = .obj_ui
	MOC_DIR     = .moc_ui
	UI_DIR      = .ui_ui
}

QT += widgets xml
