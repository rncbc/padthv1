# project(padthv1)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

configure_file (cmake_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set (HEADERS
  padthv1.h
  padthv1_sample.h
  padthv1_config.h
  padthv1_filter.h
  padthv1_formant.h
  padthv1_wave.h
  padthv1_ramp.h
  padthv1_list.h
  padthv1_fx.h
  padthv1_reverb.h
  padthv1_param.h
  padthv1_sched.h
  padthv1_tuning.h
  padthv1_programs.h
  padthv1_controls.h
)

set (SOURCES
  padthv1.cpp
  padthv1_sample.cpp
  padthv1_config.cpp
  padthv1_formant.cpp
  padthv1_wave.cpp
  padthv1_param.cpp
  padthv1_sched.cpp
  padthv1_tuning.cpp
  padthv1_programs.cpp
  padthv1_controls.cpp
)

if (QT_VERSION VERSION_LESS 5.15.0)
  qt5_wrap_cpp (MOC_SOURCES ${HEADERS})
else ()
  qt_wrap_cpp (MOC_SOURCES ${HEADERS})
endif()


set (HEADERS_UI
  padthv1_ui.h
  padthv1widget.h
  padthv1widget_env.h
  padthv1widget_filt.h
  padthv1widget_sample.h
  padthv1widget_wave.h
  padthv1widget_param.h
  padthv1widget_keybd.h
  padthv1widget_preset.h
  padthv1widget_status.h
  padthv1widget_programs.h
  padthv1widget_controls.h
  padthv1widget_control.h
  padthv1widget_palette.h
  padthv1widget_config.h
)

set (SOURCES_UI
  padthv1_ui.cpp
  padthv1widget.cpp
  padthv1widget_env.cpp
  padthv1widget_filt.cpp
  padthv1widget_sample.cpp
  padthv1widget_wave.cpp
  padthv1widget_param.cpp
  padthv1widget_keybd.cpp
  padthv1widget_preset.cpp
  padthv1widget_status.cpp
  padthv1widget_programs.cpp
  padthv1widget_controls.cpp
  padthv1widget_control.cpp
  padthv1widget_palette.cpp
  padthv1widget_config.cpp
)

set (FORMS_UI
  padthv1widget.ui
  padthv1widget_control.ui
  padthv1widget_palette.ui
  padthv1widget_config.ui
)

set (RESOURCES_UI
  padthv1.qrc
)

if (QT_VERSION VERSION_LESS 5.15.0)
  qt5_wrap_ui (UI_SOURCES_UI ${FORMS_UI})
  qt5_wrap_cpp (MOC_SOURCES_UI ${HEADERS_UI})
  qt5_add_resources (QRC_SOURCES_UI ${RESOURCES_UI})
else ()
  qt_wrap_ui (UI_SOURCES_UI ${FORMS_UI})
  qt_wrap_cpp (MOC_SOURCES_UI ${HEADERS_UI})
  qt_add_resources (QRC_SOURCES_UI ${RESOURCES_UI})
endif ()


set (HEADERS_LV2
  padthv1_lv2.h
  padthv1_lv2ui.h
  padthv1widget_lv2.h
)

set (SOURCES_LV2
  padthv1_lv2.cpp
  padthv1_lv2ui.cpp
  padthv1widget_lv2.cpp
)

if (QT_VERSION VERSION_LESS 5.15.0)
  qt5_wrap_cpp (MOC_SOURCES_LV2 ${HEADERS_LV2})
else ()
  qt_wrap_cpp (MOC_SOURCES_LV2 ${HEADERS_LV2})
endif ()


set (HEADERS_JACK
  padthv1_nsm.h
  padthv1_jack.h
  padthv1widget_jack.h
)

set (SOURCES_JACK
  padthv1_nsm.cpp
  padthv1_jack.cpp
  padthv1widget_jack.cpp
)

if (QT_VERSION VERSION_LESS 5.15.0)
  qt5_wrap_cpp (MOC_SOURCES_JACK ${HEADERS_JACK})
else ()
  qt_wrap_cpp (MOC_SOURCES_JACK ${HEADERS_JACK})
endif ()


add_library (${PROJECT_NAME} STATIC
  ${MOC_SOURCES}
  ${SOURCES}
)

add_library (${PROJECT_NAME}_ui STATIC
  ${UI_SOURCES_UI}
  ${MOC_SOURCES_UI}
  ${QRC_SOURCES_UI}
  ${SOURCES_UI}
)

add_library (${PROJECT_NAME}_lv2 MODULE
  ${MOC_SOURCES_LV2}
  ${SOURCES_LV2}
)

add_executable (${PROJECT_NAME}_jack
  ${MOC_SOURCES_JACK}
  ${SOURCES_JACK}
)


set_target_properties (${PROJECT_NAME}       PROPERTIES CXX_STANDARD 17)
set_target_properties (${PROJECT_NAME}_ui    PROPERTIES CXX_STANDARD 17)
set_target_properties (${PROJECT_NAME}_lv2   PROPERTIES CXX_STANDARD 17)
set_target_properties (${PROJECT_NAME}_jack  PROPERTIES CXX_STANDARD 17)

target_link_libraries (${PROJECT_NAME}       PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Xml)
target_link_libraries (${PROJECT_NAME}_ui    PUBLIC Qt${QT_VERSION_MAJOR}::Widgets ${PROJECT_NAME})
target_link_libraries (${PROJECT_NAME}_lv2   PRIVATE ${PROJECT_NAME}_ui)
target_link_libraries (${PROJECT_NAME}_jack  PRIVATE ${PROJECT_NAME}_ui)

if (CONFIG_FFTW3)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::FFTW3)
endif ()

if (CONFIG_JACK)
  target_link_libraries (${PROJECT_NAME}_jack PRIVATE PkgConfig::JACK)
endif ()

if (CONFIG_ALSA_MIDI)
  target_link_libraries (${PROJECT_NAME}_jack PRIVATE PkgConfig::ALSA)
endif ()

if (CONFIG_LIBLO)
  target_link_libraries (${PROJECT_NAME}_jack PRIVATE PkgConfig::LIBLO)
endif ()


if (UNIX AND NOT APPLE AND NOT CONFIG_DEBUG)
  add_custom_command(TARGET ${PROJECT_NAME}_lv2  POST_BUILD
    COMMAND strip lib${PROJECT_NAME}_lv2.so)
  add_custom_command(TARGET ${PROJECT_NAME}_jack POST_BUILD
    COMMAND strip ${PROJECT_NAME}_jack)
endif ()


if (UNIX AND NOT APPLE)
  if (CONFIG_PREFIX MATCHES $ENV{HOME})
    set (CONFIG_LV2DIR ${CONFIG_PREFIX}/.lv2)
  else ()
    set (CONFIG_LV2DIR ${CONFIG_LIBDIR}/lv2)
  endif ()
  install (FILES ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}_lv2.so
     RENAME ${PROJECT_NAME}.so
     DESTINATION ${CONFIG_LV2DIR}/${PROJECT_NAME}.lv2)
  install (FILES ${PROJECT_NAME}.lv2/manifest.ttl
     ${PROJECT_NAME}.lv2/${PROJECT_NAME}.ttl
     ${PROJECT_NAME}.lv2/${PROJECT_NAME}_ui.ttl
     DESTINATION ${CONFIG_LV2DIR}/${PROJECT_NAME}.lv2)
  install (TARGETS ${PROJECT_NAME}_jack RUNTIME
     DESTINATION ${CMAKE_INSTALL_BINDIR})
  install (FILES ${PROJECT_NAME}.desktop
     DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install (FILES images/${PROJECT_NAME}.png
     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps)
  install (FILES images/${PROJECT_NAME}.svg
     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
  install (FILES appdata/${PROJECT_NAME}.appdata.xml
     DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  install (FILES mimetypes/${PROJECT_NAME}.xml
     DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
  install (FILES mimetypes/application-x-${PROJECT_NAME}-preset.png
     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/mimetypes)
  install (FILES mimetypes/application-x-${PROJECT_NAME}-preset.svg
     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/mimetypes)
endif ()
