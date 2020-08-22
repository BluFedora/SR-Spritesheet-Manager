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
  Source/Data/srsm_animation.cpp \
  Source/Data/srsm_project.cpp \
  Source/Data/srsm_settings.cpp \
  Source/UI/sr_welcome_window.cpp \
  Source/UI/srsm_animated_sprite.cpp \
  Source/UI/srsm_animation_preview.cpp \
  Source/UI/srsm_image_library.cpp \
  Source/UI/srsm_timeline.cpp \
  Source/main.cpp \
  Source/mainwindow.cpp \
  Source/newanimation.cpp

HEADERS  += \
    Source/Data/srsm_animation.hpp \
    Source/Data/srsm_project.hpp \
    Source/Data/srsm_settings.hpp \
    Source/UI/sr_welcome_window.hpp \
    Source/UI/srsm_animated_sprite.hpp \
    Source/UI/srsm_animation_preview.hpp \
    Source/UI/srsm_image_library.hpp \
    Source/UI/srsm_timeline.hpp \
    Source/main.hpp \
    Source/mainwindow.hpp \
    Source/newanimation.hpp

INCLUDEPATH += Source/

FORMS    += \
    Source/UI/sr_welcome_window.ui \
    Source/UI/srsm_animation_preview.ui \
    Source/UI/srsm_timeline.ui \
    Source/mainwindow.ui \
    Source/newanimation.ui

# warn on *any* usage of deprecated APIs, no matter in which Qt version they got marked as deprecated ...
DEFINES += QT_DEPRECATED_WARNINGS

# Fail to compile if APIs deprecated in Qt <= 5.14.0 are used

# I am using "QWheelEvent::delta" which is deprecated as of 5.15

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=331264

DISTFILES +=

RESOURCES += \
  Resources/ResourceFile.qrc

win32:CONFIG(release, debug|release): LIBS += -LE:/Repos/bfEngine/bin/ -lbf.Animation2DDLL
else:win32:CONFIG(debug, debug|release): LIBS += -LE:/Repos/bfEngine/bin/ -lbf.Animation2DDLLd
else:unix: LIBS += -LE:/Repos/bfEngine/bin/ -lbf.Animation2DDLL

INCLUDEPATH += E:/Repos/bfEngine/Bifrost/Anim2D/include
DEPENDPATH += E:/Repos/bfEngine/BifrostEngine/bin
