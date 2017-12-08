TARGET = "hdf-explorer"
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
HEADERS = hdf_explorer.hpp visit.hpp iterate.hpp
SOURCES = hdf_explorer.cpp visit.cpp iterate.cpp
RESOURCES = hdf_explorer.qrc
ICON = sample.icns
RC_FILE = hdf_explorer.rc

unix:!macx {
 LIBS += -lhdf5
}

macx: {
 INCLUDEPATH += /usr/local/include
 LIBS += /usr/local/lib/libhdf5.a
 LIBS += /usr/local/lib/libhdf5_hl.a
 LIBS += /usr/local/lib/libsz.a
 LIBS += -lcurl -lz
}

