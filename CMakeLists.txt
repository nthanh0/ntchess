cmake_minimum_required(VERSION 3.16)

project(ntchess VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Multimedia Concurrent)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appntchess
    src/main.cpp
    src/backend/Board.cpp
    src/backend/Board.h
    src/backend/Game.cpp
    src/backend/Game.h
    src/backend/misc.cpp
    src/backend/misc.h
    src/backend/movegen.cpp
    src/backend/movegen.h
    src/backend/notation.cpp
    src/backend/notation.h
    src/backend/types.h
    src/backend/uci_engine.cpp
    src/backend/uci_engine.h
    src/bridge/GameBridge.cpp
    src/bridge/GameBridge.h
)

target_compile_definitions(appntchess PRIVATE
    # Development fallback: when running from the build directory the assets
    # haven't been copied yet, so the app falls back to this source-tree path.
    ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets"
)

# ── Install rules ────────────────────────────────────────────────────────────
# Copy the entire assets folder next to the installed executable so the app
# is fully self-contained (no system-wide data directories needed).
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)

if(WIN32)
    # Locate windeployqt next to qmake / in the Qt bin directory.
    find_program(WINDEPLOYQT windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
              "$ENV{QT_ROOT_DIR}/bin"
    )

    if(WINDEPLOYQT)
        # ── Post-build: copy Qt DLLs into the build directory ───────────────
        # This lets you run the app directly from the build folder without
        # having to run cmake --install first.
        add_custom_command(TARGET appntchess POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --qmldir "${CMAKE_SOURCE_DIR}/qml"
                    "$<TARGET_FILE:appntchess>"
            COMMENT "Running windeployqt (post-build)..."
            VERBATIM
        )

        # ── Install: same thing for the install prefix ───────────────────────
        install(CODE "
            execute_process(
                COMMAND \"${WINDEPLOYQT}\"
                        --no-translations
                        --no-system-d3d-compiler
                        --no-opengl-sw
                        --qmldir \"${CMAKE_SOURCE_DIR}/qml\"
                        \"\${CMAKE_INSTALL_PREFIX}/appntchess.exe\"
            )
        ")
    else()
        message(WARNING
            "windeployqt not found. Qt DLLs will not be bundled automatically.\n"
            "Set QT_ROOT_DIR to your Qt MinGW kit root (e.g. C:/Qt/6.10.0/mingw_64) "
            "or add the Qt bin directory to PATH."
        )
    endif()
endif()

target_include_directories(appntchess PRIVATE
    ${CMAKE_SOURCE_DIR}/src/bridge
)

qt_add_qml_module(appntchess
    URI ntchess
    QML_FILES
        qml/Main.qml
        qml/ChessBoard.qml
        qml/SettingsDialog.qml
        qml/NewGameDialog.qml
        qml/ImportDialog.qml
        qml/AnalysisSettingsDialog.qml
        qml/AnalysisView.qml
        qml/PlayerBar.qml
        qml/IconBtn.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appntchess PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appntchess
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appntchess
    PRIVATE Qt6::Quick Qt6::Multimedia Qt6::Concurrent
)

install(TARGETS appntchess
    BUNDLE  DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION .
)