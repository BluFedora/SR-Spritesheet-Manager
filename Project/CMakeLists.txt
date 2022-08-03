

project(SRSpritesheetManager VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets)

if(QT_FOUND)
  find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Network OpenGLWidgets REQUIRED)

  qt_add_executable(
    SRSpritesheetManager
      MANUAL_FINALIZATION
      "Source/Data/bf_property.hpp"
      "Source/Data/sr_animation.hpp"
      "Source/Data/sr_project.hpp"
      "Source/Data/sr_settings.hpp"
      "Source/Server/sr_live_reload_server.hpp"
      "Source/UI/sr_animated_sprite.hpp"
      "Source/UI/sr_animation_preview.hpp"
      "Source/UI/sr_image_library.hpp"
      "Source/UI/sr_timeline.hpp"
      "Source/UI/sr_welcome_window.hpp"
      "Source/sr_main_window.hpp"
      "Source/sr_new_animation_dialog.hpp"

      "Source/Data/sr_animation.cpp"
      "Source/Data/sr_project.cpp"
      "Source/Data/sr_settings.cpp"
      "Source/Server/sr_live_reload_server.cpp"
      "Source/UI/sr_animated_sprite.cpp"
      "Source/UI/sr_animation_preview.cpp"
      "Source/UI/sr_image_library.cpp"
      "Source/UI/sr_timeline.cpp"
      "Source/UI/sr_welcome_window.cpp"
      "Source/main.cpp"
      "Source/sr_main_window.cpp"
      "Source/sr_new_animation_dialog.cpp"

      "Source/UI/sr_animation_preview.ui"
      "Source/UI/sr_timeline.ui"
      "Source/UI/sr_welcome_window.ui"
      "Source/sr_main_window.ui"
      "Source/sr_new_animation_dialog.ui"

      "Resources/ResourceFile.qrc"
  )

  target_include_directories(
    SRSpritesheetManager

    PRIVATE
      "Source"
  )

  target_link_libraries(
    SRSpritesheetManager
    PRIVATE
      Qt${QT_VERSION_MAJOR}::Widgets
      Qt${QT_VERSION_MAJOR}::Network
      Qt${QT_VERSION_MAJOR}::OpenGLWidgets
      bfAnimation2D_shared
  )

  set_target_properties(SRSpritesheetManager PROPERTIES
      MACOSX_BUNDLE_GUI_IDENTIFIER sr.spritesheet_manager.com
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
  )

  if(QT_VERSION_MAJOR EQUAL 6)
      qt_finalize_executable(SRSpritesheetManager)
  endif()
endif()

