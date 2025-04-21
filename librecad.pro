TEMPLATE = subdirs
TARGET = librecad
CONFIG += ordered
CONFIG += c++17
CONFIG += rtti
SUBDIRS     = \
    libraries \
    librecad \
    plugins \
    tools

# c++17 is now obligatory for LibreCAD
message(We will be using CPP17 features)
#QMAKE_CXXFLAGS += -O2 -std=c++17
# Fix: Only use /std:c++17 for MSVC, and -std=c++17 for others
win32:msvc* {
    QMAKE_CXXFLAGS += /std:c++17 /O2
} else {
    QMAKE_CXXFLAGS += -std=c++17 -O2
}
#QMAKE_CXXFLAGS += -O2 -std=c++17
exists(custom.pro):include( custom.pro )

OTHER_FILES = \
    CHANGELOG.md \
    README.md \
    desktop/librecad.desktop \
    desktop/org.librecad.librecad.appdata.xml

