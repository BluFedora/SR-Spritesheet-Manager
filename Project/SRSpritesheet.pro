#-------------------------------------------------
#
# Project created by QtCreator 2017-12-27T16:32:29
#
#-------------------------------------------------

QT += core widgets gui network

TARGET   = SRSpritesheetManager
TEMPLATE = app

CONFIG += c++17

SOURCES += \
  Source/Data/sr_animation.cpp \
  Source/Data/sr_project.cpp \
  Source/Data/sr_settings.cpp \
  Source/Server/sr_live_reload_server.cpp \
  Source/UI/sr_animated_sprite.cpp \
  Source/UI/sr_animation_preview.cpp \
  Source/UI/sr_image_library.cpp \
  Source/UI/sr_timeline.cpp \
  Source/UI/sr_welcome_window.cpp \
  Source/main.cpp \
  Source/sr_main_window.cpp \
  Source/sr_new_animation_dialog.cpp

HEADERS  += \
    Source/Data/bf_property.hpp \
    Source/Data/sr_animation.hpp \
    Source/Data/sr_project.hpp \
    Source/Data/sr_settings.hpp \
    Source/Server/sr_live_reload_server.hpp \
    Source/UI/sr_animated_sprite.hpp \
    Source/UI/sr_animation_preview.hpp \
    Source/UI/sr_image_library.hpp \
    Source/UI/sr_timeline.hpp \
    Source/UI/sr_welcome_window.hpp \
    Source/sr_main_window.hpp \
    Source/sr_new_animation_dialog.hpp

INCLUDEPATH += Source/

FORMS    += \
    Source/UI/sr_animation_preview.ui \
    Source/UI/sr_timeline.ui \
    Source/UI/sr_welcome_window.ui \
    Source/sr_main_window.ui \
    Source/sr_new_animation_dialog.ui

# warn on *any* usage of deprecated APIs, no matter in which Qt version they got marked as deprecated ...
DEFINES += QT_DEPRECATED_WARNINGS

# Fail to compile if APIs deprecated in Qt <= 5.14.0 are used

# I am using "QWheelEvent::delta" which is deprecated as of 5.15

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=331264

DISTFILES +=

RESOURCES += \
  Resources/ResourceFile.qrc

LIBS += -LC:/Repos/bfEngine/bin/

win32:CONFIG(release, debug|release): LIBS += -lbf.Animation2DDLL
else:win32:CONFIG(debug, debug|release): LIBS += -lbf.Animation2DDLL
else:unix: LIBS += -lbf.Animation2DDLL

INCLUDEPATH += C:/Repos/bfEngine/Engine/Anim2D/include
DEPENDPATH += C:/Repos/bfEngine/BifrostEngine/bin
