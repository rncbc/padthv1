# padthv1_jack.pro
#
NAME = padthv1

TARGET = $${NAME}_jack
TEMPLATE = app

include(src_jack.pri)

HEADERS = \
	config.h \
	padthv1.h \
	padthv1_ui.h \
	padthv1_config.h \
	padthv1_param.h \
	padthv1_programs.h \
	padthv1_controls.h \
	padthv1_nsm.h \
	padthv1_jack.h \
	padthv1widget.h \
	padthv1widget_env.h \
	padthv1widget_filt.h \
	padthv1widget_sample.h \
	padthv1widget_wave.h \
	padthv1widget_param.h \
	padthv1widget_preset.h \
	padthv1widget_status.h \
	padthv1widget_programs.h \
	padthv1widget_controls.h \
	padthv1widget_control.h \
	padthv1widget_config.h \
	padthv1widget_jack.h

SOURCES = \
	padthv1_nsm.cpp \
	padthv1_jack.cpp \
	padthv1widget.cpp \
	padthv1widget_env.cpp \
	padthv1widget_filt.cpp \
	padthv1widget_sample.cpp \
	padthv1widget_wave.cpp \
	padthv1widget_param.cpp \
	padthv1widget_preset.cpp \
	padthv1widget_status.cpp \
	padthv1widget_programs.cpp \
	padthv1widget_controls.cpp \
	padthv1widget_control.cpp \
	padthv1widget_config.cpp \
	padthv1widget_jack.cpp

FORMS = \
	padthv1widget.ui \
	padthv1widget_control.ui \
	padthv1widget_config.ui

RESOURCES += padthv1.qrc


unix {

	OBJECTS_DIR = .obj_jack
	MOC_DIR     = .moc_jack
	UI_DIR      = .ui_jack

	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}

	isEmpty(BINDIR) {
		BINDIR = $${PREFIX}/bin
	}

	isEmpty(LIBDIR) {
		TARGET_ARCH = $$system(uname -m)
		contains(TARGET_ARCH, x86_64) {
			LIBDIR = $${PREFIX}/lib64
		} else {
			LIBDIR = $${PREFIX}/lib
		}
	}

	isEmpty(DATADIR) {
		DATADIR = $${PREFIX}/share
	}

	#DEFINES += DATADIR=\"$${DATADIR}\"

	INSTALLS += target desktop icon appdata \
		icon_scalable mimeinfo mimetypes mimetypes_scalable

	target.path = $${BINDIR}

	desktop.path = $${DATADIR}/applications
	desktop.files += $${NAME}.desktop

	icon.path = $${DATADIR}/icons/hicolor/32x32/apps
	icon.files += images/$${NAME}.png 

	icon_scalable.path = $${DATADIR}/icons/hicolor/scalable/apps
	icon_scalable.files += images/$${NAME}.svg

	appdata.path = $${DATADIR}/metainfo
	appdata.files += appdata/$${NAME}.appdata.xml

	mimeinfo.path = $${DATADIR}/mime/packages
	mimeinfo.files += mimetypes/$${NAME}.xml

	mimetypes.path = $${DATADIR}/icons/hicolor/32x32/mimetypes
	mimetypes.files += mimetypes/application-x-$${NAME}-preset.png

	mimetypes_scalable.path = $${DATADIR}/icons/hicolor/scalable/mimetypes
	mimetypes_scalable.files += mimetypes/application-x-$${NAME}-preset.svg

	LIBS += -L. -l$${NAME} -Wl,-rpath,$${LIBDIR}
}

QT += xml

# QT5 support
!lessThan(QT_MAJOR_VERSION, 5) {
	QT += widgets
}
