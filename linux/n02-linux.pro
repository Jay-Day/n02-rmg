QT += core widgets network
CONFIG += c++17

TARGET = n02-gui
TEMPLATE = app

# Include paths
INCLUDEPATH += src src/common include

# Core library sources
SOURCES += \
    src/main.cpp \
    src/common/k_socket.cpp \
    src/common/k_framecache.cpp \
    src/common/settings.cpp \
    src/common/timer.cpp \
    src/core/p2p/p2p_instruction.cpp \
    src/core/p2p/p2p_message.cpp \
    src/core/p2p/p2p_core.cpp \
    src/core/kaillera/k_instruction.cpp \
    src/core/kaillera/k_message.cpp \
    src/core/kaillera/kaillera_core.cpp \
    src/core/playback/player.cpp \
    src/core/module_manager.cpp \
    src/ui/mainwindow.cpp \
    src/ui/p2p/p2p_selection_dialog.cpp \
    src/ui/p2p/p2p_connection_dialog.cpp \
    src/ui/widgets/chat_widget.cpp \
    src/ui/kaillera/kaillera_server_select_dialog.cpp \
    src/ui/kaillera/kaillera_server_dialog.cpp \
    src/ui/kaillera/kaillera_master_dialog.cpp

HEADERS += \
    src/common/k_socket.h \
    src/common/k_framecache.h \
    src/common/containers.h \
    src/common/stats.h \
    src/common/settings.h \
    src/common/timer.h \
    src/core/p2p/p2p_instruction.h \
    src/core/p2p/p2p_message.h \
    src/core/p2p/p2p_core.h \
    src/core/kaillera/k_instruction.h \
    src/core/kaillera/k_message.h \
    src/core/kaillera/kaillera_core.h \
    src/core/playback/player.h \
    src/core/module_manager.h \
    src/ui/mainwindow.h \
    src/ui/p2p/p2p_selection_dialog.h \
    src/ui/p2p/p2p_connection_dialog.h \
    src/ui/widgets/chat_widget.h \
    src/ui/kaillera/kaillera_server_select_dialog.h \
    src/ui/kaillera/kaillera_server_dialog.h \
    src/ui/kaillera/kaillera_master_dialog.h

# Output directory
DESTDIR = build
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
