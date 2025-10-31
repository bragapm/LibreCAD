#!/bin/bash -x
#

## script to build LibreCAD AppImage
## intended to be used on Travis CI
## does not run on modern systems because of linuxdeployqt
## for testing it can be called with parameter 'clean' to remove AppImage file and folder

# ensure that the script is called from LibreCAD root folder and executable exists
if [ ! -d "unix" ]; then
    echo "The script has to be called from LibreCAD root folder!"
    exit
fi
if [ ! -f "unix/librecad" ]; then
    echo "Please build LibreCAD first, before calling this script!"
    exit
fi

# for testing purposes and manual use
# this script can be called with parameter clean
# to remove appdir and LibreCAD*.AppImage files
if [ 1 -eq $# ] && [ "clean" = "$1" ]; then
    [ -d "appdir" ] && rm -Rf appdir
    compgen -G "LibreCAD*.AppImage" >/dev/null && rm LibreCAD*.AppImage
    echo "cleaned LibreCAD AppImage files"
fi

# create folder structure
mkdir -p appdir/usr/bin
mkdir -p appdir/usr/share/applications
mkdir -p appdir/usr/share/librecad
mkdir -p appdir/usr/share/metainfo
mkdir -p appdir/usr/share/doc/librecad
mkdir -p appdir/usr/share/icons/hicolor/256x256/apps
mkdir -p appdir/usr/share/icons/hicolor/128x128/apps
mkdir -p appdir/usr/share/icons/hicolor/64x64/apps
mkdir -p appdir/usr/share/icons/hicolor/32x32/apps
mkdir -p appdir/usr/share/icons/hicolor/16x16/apps
mkdir -p appdir/usr/share/librecad/qm

# strip binaries
strip unix/librecad
strip unix/resources/plugins/*.so

# copy executables and binary resources
cp unix/librecad appdir/usr/bin/

cp -r unix/ECWJP2Reader appdir/usr/bin
cp unix/template.json appdir/usr/bin
cp unix/bintang.dxf appdir/usr/bin
cp unix/gusutemplate.dxf appdir/usr/bin
cp unix/gusutemplate.dwt appdir/usr/bin

# cp -r unix/resources appdir/usr/lib/
# cp -r unix/resources/plugins appdir/usr/share/librecad


SOURCE_PLUG_DIR=unix/resources/plugins/

DEPLOY_PLUG_DIR=appdir/usr/bin/resources/plugins/
# DEPLOY_PLUG_DIR=appdir/usr/lib/librecad/
# DEPLOY_PLUG_DIR=appdir/usr/plugins/librecad/

FINAL_PLUG_DIR=appdir/usr/bin/resources/plugins/
# FINAL_PLUG_DIR=$DEPLOY_PLUG_DIR

mkdir -p $FINAL_PLUG_DIR
mkdir -p $DEPLOY_PLUG_DIR
cp ${SOURCE_PLUG_DIR}*.so $DEPLOY_PLUG_DIR

mkdir -p appdir/usr/lib/librecad/
cp ${SOURCE_PLUG_DIR}*.so appdir/usr/lib/librecad/

cp desktop/librecad.desktop appdir/usr/share/applications/
cp desktop/org.librecad.librecad.appdata.xml appdir/usr/share/metainfo/

cp -r librecad/support/doc/* appdir/usr/share/doc/librecad/
cp -r librecad/support/fonts appdir/usr/share/librecad/
cp -r librecad/support/library appdir/usr/share/librecad/
cp -r librecad/support/patterns appdir/usr/share/librecad/

convert librecad/res/images/tataletak.png -resize 256x256 appdir/usr/share/icons/hicolor/256x256/apps/tataletak.png
convert librecad/res/images/tataletak.png -resize 128x128 appdir/usr/share/icons/hicolor/128x128/apps/tataletak.png
convert librecad/res/images/tataletak.png -resize 64x64 appdir/usr/share/icons/hicolor/64x64/apps/tataletak.png
convert librecad/res/images/tataletak.png -resize 32x32 appdir/usr/share/icons/hicolor/32x32/apps/tataletak.png
convert librecad/res/images/tataletak.png -resize 16x16 appdir/usr/share/icons/hicolor/16x16/apps/tataletak.png

wget -nc https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/latest/download/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy-plugin-qt-x86_64.AppImage

wget -nc https://github.com/linuxdeploy/linuxdeploy/releases/latest/download/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

# remove problematic sql drivers
rm -f ~/Qt/6.9.1/gcc_64/plugins/sqldrivers/libqsqlmimer.so
rm -f ~/Qt/6.9.1/gcc_64/plugins/sqldrivers/libqsqlmysql.so
rm -f ~/Qt/6.9.1/gcc_64/plugins/sqldrivers/libqsqlpsql.so
rm -f ~/Qt/6.9.1/gcc_64/plugins/sqldrivers/libqsqlodbc.so

export LINUXDEPLOY_OUTPUT_APP_NAME=LibreCAD-Tataletak
export QMAKE=~/Qt/6.9.1/gcc_64/bin/qmake
export NO_STRIP=1
export DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1
# export EXTRA_QT_MODULES='concurrent;opengl;openglwidgets;sql;uitools'
./linuxdeploy-x86_64.AppImage \
    --appdir ./appdir \
    --executable ./appdir/usr/bin/librecad \
    --desktop-file ./appdir/usr/share/applications/librecad.desktop \
    --icon-file ./appdir/usr/share/icons/hicolor/256x256/apps/tataletak.png \
    --plugin qt

EXCLUDES=(
    "libcap.so*"
    "libpcre*.so*"
    "libselinux.so*"
    "libsystemd.so*"
    "libkrb5*.so*"
    "libk5crypto.so*"
    "libgomp.so*"
    "libgssapi*.so*"
    "libzstd.so*"
    "libxkb*.so*"
    #"libxcb*.so*"
    "libgthread-2.0.so*"
    "libglib-2.0.so*"
    "libssl.so*"
    "libcrypto.so*"
    "libmuparser.so*"
    "libcups.so*"
)

# Remove the excluded files
for lib in "${EXCLUDES[@]}"; do
    rm -f appdir/usr/lib/$lib
done

rm -r appdir/usr/lib/librecad/
mv ${DEPLOY_PLUG_DIR}*.so $FINAL_PLUG_DIR
# rm -r ${DEPLOY_PLUG_DIR}
PLUG_TO_LIB=$(realpath --relative-to=$FINAL_PLUG_DIR appdir/usr/lib)
find $FINAL_PLUG_DIR -type f -name "*.so*" | while read -r file; do
    echo "Patching RPATH for: $file"
    patchelf --set-rpath "\$ORIGIN/$PLUG_TO_LIB:\$ORIGIN" "$file"
done


deploy_lib() {
    local libname="$1"
    local target_dir=appdir/usr/lib

    lib_dir=$( ldconfig -p | grep $libname | awk '{print $4}' | head -n1 )
    cp $lib_dir $target_dir/$libname
    patchelf --set-rpath '$ORIGIN' $target_dir/$libname
}
# some weird behaving dependencies
deploy_lib libmuparser.so.2
deploy_lib libssl.so.3
deploy_lib libcrypto.so.3


cp -f CI/AppRun appdir/AppRun
chmod +x appdir/AppRun

patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN/usr/lib:$ORIGIN/usr/bin:$ORIGIN' appdir/AppRun.wrapped

# EXCLUDE_ARGS=()
# for lib in "${EXCLUDES[@]}"; do
#     EXCLUDE_ARGS+=( "--exclude-library" "$lib" )
# done
# ./linuxdeploy-x86_64.AppImage \
#     --appdir ./appdir \
#     --output appimage \
#     "${EXCLUDE_ARGS[@]}"

wget -nc https://github.com/$(wget -q https://github.com/probonopd/go-appimage/releases/expanded_assets/continuous -O - | grep "appimagetool-.*-x86_64.AppImage" | head -n 1 | cut -d '"' -f 2)
chmod +x appimagetool-*.AppImage
VERSION=Tataletak ./appimagetool-*.AppImage ./appdir

# ./LibreCAD-Tataletak-x86_64.AppImage
# LD_DEBUG=libs appdir/AppRun
# LD_DEBUG=libs ./LibreCAD-Tataletak-x86_64.AppImage
# ./LibreCAD-Tataletak-x86_64.AppImage
