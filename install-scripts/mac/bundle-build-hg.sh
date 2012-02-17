#!/bin/bash -e
#
# Build the orange Mac OSX bundle
#
# ./bundle-build-hg.sh work_dir revision bundle_output_file
# ./bundle-build-hg.sh /private/tmp tip /private/tmp/orange-bundle-hg-tip.dmg
#

WORK_DIR=$1
REVISION=$2
BUNDLE=$3

TMP_BUNDLE_DIR=${WORK_DIR}/bundle
REPOS_DIR=${WORK_DIR}/repos

# Remove leftovers if any
if [ -e $TMP_BUNDLE_DIR ]; then
	rm -rf $TMP_BUNDLE_DIR
fi

# Preapare the bundle template
svn export --non-interactive http://orange.biolab.si/svn/orange/externals/trunk/install-scripts/mac/bundle/ $TMP_BUNDLE_DIR

# Make repos dir if it does not yet exist
if [ ! -e $REPOS_DIR ]; then
	mkdir $REPOS_DIR
fi

echo "Checkouting and building orange"
./bundle-inject-hg.sh https://bitbucket.org/biolab/orange orange $REVISION $REPOS_DIR ${TMP_BUNDLE_DIR}/Orange.app

echo "Checkouting and building bioinformatics addon"
./bundle-inject-hg.sh https://bitbucket.org/biolab/orange-addon-bioinformatics bioinformatics $REVISION $REPOS_DIR ${TMP_BUNDLE_DIR}/Orange.app

echo "Checkouting and building text addon"
./bundle-inject-hg.sh https://bitbucket.org/biolab/orange-addon-text text $REVISION $REPOS_DIR ${TMP_BUNDLE_DIR}/Orange.app

echo "Removing unnecessary files."
find $TMP_BUNDLE_DIR \( -name '*~' -or -name '*.bak' -or -name '*.pyc' -or -name '*.pyo' -or -name '*.pyd' \) -exec rm -rf {} ';'

	
# Prepare the .dmg image

# Makes a link to Applications folder
ln -s /Applications/ $TMP_BUNDLE_DIR/Applications

echo "Fixing bundle permissions."

{ chown -Rh root:wheel $TMP_BUNDLE_DIR; } || { echo "Could not fix bundle permissions"; }

echo "Creating temporary image with the bundle."

TMP_BUNDLE=${WORK_DIR}/bundle.dmg
rm -f $TMP_BUNDLE

hdiutil detach /Volumes/Orange -force || true
hdiutil create -format UDRW -volname Orange -fs HFS+ -fsargs "-c c=64,a=16,e=16" -srcfolder $TMP_BUNDLE_DIR $TMP_BUNDLE
MOUNT_OUTPUT=`hdiutil attach -readwrite -noverify -noautoopen $TMP_BUNDLE | egrep '^/dev/'`
DEV_NAME=`echo -n "$MOUNT_OUTPUT" | head -n 1 | awk '{print $1}'`
MOUNT_POINT=`echo -n "$MOUNT_OUTPUT" | tail -n 1 | awk '{print $3}'`

# Makes the disk image window open automatically when mounted
bless -openfolder "$MOUNT_POINT"
# Hides background directory even more
/Developer/Tools/SetFile -a V "$MOUNT_POINT/.background/"
# Sets the custom icon volume flag so that volume has nice Orange icon after mount (.VolumeIcon.icns)
/Developer/Tools/SetFile -a C "$MOUNT_POINT"

# Might mot have permissions to do this
{ rm -rf "$MOUNT_POINT/.Trashes/"; } || { echo "Could not remove $MOUNT_POINT/.Trashes/"; }

{ rm -rf "$MOUNT_POINT/.fseventsd/"; } || { echo "Could not remove $MOUNT_POINT/.fseventsd/"; }

hdiutil detach "$DEV_NAME" -force

echo "Converting temporary image to a compressed image."

if [ -e $BUNDLE ]; then
	rm -f $BUNDLE
fi

hdiutil convert $TMP_BUNDLE -format UDZO -imagekey zlib-level=9 -o $BUNDLE

echo "Cleaning up."
rm -f $TMP_BUNDLE
rm -rf $TMP_BUNDLE_DIR

true