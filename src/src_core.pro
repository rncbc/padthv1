# padthv1.pro
#
NAME = padthv1

TARGET = $${NAME}
TEMPLATE = lib
CONFIG += static

include(src_core.pri)

HEADERS = \
	config.h \
	padthv1.h \
	padthv1_sample.h \
	padthv1_config.h \
	padthv1_filter.h \
	padthv1_formant.h \
	padthv1_wave.h \
	padthv1_ramp.h \
	padthv1_list.h \
	padthv1_fx.h \
	padthv1_reverb.h \
	padthv1_param.h \
	padthv1_sched.h \
	padthv1_tuning.h \
	padthv1_programs.h \
	padthv1_controls.h

SOURCES = \
	padthv1.cpp \
	padthv1_sample.cpp \
	padthv1_config.cpp \
	padthv1_formant.cpp \
	padthv1_wave.cpp \
	padthv1_param.cpp \
	padthv1_sched.cpp \
	padthv1_tuning.cpp \
	padthv1_programs.cpp \
	padthv1_controls.cpp


unix {

	OBJECTS_DIR = .obj_core
	MOC_DIR     = .moc_core
	UI_DIR      = .ui_core
}

QT -= gui
QT += xml
