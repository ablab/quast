## createdist.sh
## This is a shell script to automatically create a gnuplot distribution 
## for Mac OS X. It will download and build a source tarball and create
## a disk image with a standard GUI installer and folders with supporting
## files (ReadMe, docs in PDF and demos).
##
## The utility "freeze" from the application Iceberg is required, see
## http://s.sudre.free.fr/Software/Iceberg.html
##
## 2004-10-21 Per Persson, persquare@users.sf.net

## Change as needed
VERSION="4.0.0"
SHORTVER="4.0"
BETA=
DISTRO_DIR=`pwd`
## No changes needed below this line

echo "==== Creating distro of gnuplot $VERSION$BETA"

TARBALL="gnuplot-$VERSION.tar.gz"
WORK_DIR="$DISTRO_DIR/work"
SRC_DIR="$WORK_DIR/gnuplot-$VERSION"
BUILD_DIR="$DISTRO_DIR/build"
PRODUCTS_DIR="$BUILD_DIR/products"
DMG_ROOT="$BUILD_DIR/dmg_root"

## Always remove product
rm -rf $BUILD_DIR/Gnuplot-$VERSION$BETA.dmg 

if [ "$1" = "pack" ]; then
    echo "*** Warning skipping build"
else
    echo "Setting up directories"
    rm -rf build.log
    rm -rf $BUILD_DIR
    mkdir $BUILD_DIR
    if [ ! -d $WORK_DIR ]; then
	mkdir $WORK_DIR
    else
	(cd $WORK_DIR; rm -rf $SRC_DIR)
    fi
    mkdir $PRODUCTS_DIR
    mkdir $DMG_ROOT
    mkdir $DMG_ROOT/Demo
    mkdir $DMG_ROOT/Docs

    echo "Fetching tarball to $WORK_DIR"
    if [ ! -f $WORK_DIR/$TARBALL ]; then 
	curl -o $WORK_DIR/$TARBALL http://heanet.dl.sourceforge.net/sourceforge/gnuplot/$TARBALL
    fi
    ( 
	cd $WORK_DIR
	gnutar xvzf $TARBALL
    )

    echo "Configuring in $SRC_DIR"
    (
	cd $SRC_DIR
	./configure --prefix=/usr/local \
	    --without-gd --without-png
    )
    echo "#undef'ing HAVE_STPCPY which didn't exist prior to 10.3"
    (
	cd $SRC_DIR 
	cp config.h config_10_3.h
	sed "s/#define HAVE_STPCPY 1/\/\* #undef HAVE_STPCPY \*\//g" < config.h > config_10_2.h
	cp config_10_2.h config.h
    )

    echo "Building in $SRC_DIR"
    (
	cd $SRC_DIR
	make
    )
    
    echo "Installing in $PRODUCTS_DIR"
    (
	cd $SRC_DIR
	make install DESTDIR=$PRODUCTS_DIR
    )

    echo "Copying demo files to $DMGROOT/Demo"
    cp $SRC_DIR/demo/* $DMG_ROOT/Demo/.
    rm -f $DMG_ROOT/demo/Make*
    rm -f $DMG_ROOT/demo/webify.pl

    echo "Building docs"
    (
	cd $SRC_DIR/docs
	make pdf
	make gpcard.ps
	dvipdfm -p a4 -x 1.25in gpcard.dvi
        # cd psdocs
        # make pdf
    )
    (
	cd $SRC_DIR/tutorial
	make pdf
    )

    echo "Copying docs to $DMGROOT/Docs"
    cp $SRC_DIR/docs/{gnuplot,gpcard}.pdf $DMG_ROOT/Docs/.
    cp $SRC_DIR/tutorial/tutorial.pdf $DMG_ROOT/Docs/gnuplot_LateX_tutorial.pdf
    cp $SRC_DIR/{FAQ,README,Copyright} $DMG_ROOT/Docs/.
fi ## skip build

echo "==== Making distro ===="
echo "gnuplot $VERSION$BETA for Mac OS X binary distribution" > build.log
echo "Dependencies" >> build.log 
otool -L $PRODUCTS_DIR/usr/local/bin/gnuplot | tee -a build.log
otool -L $PRODUCTS_DIR/usr/local/libexec/gnuplot/$SHORTVER/gnuplot_x11 | tee -a build.log
echo "Installer contents" >> build.log 
lsbom $DMG_ROOT/gnuplot.pkg/Contents/Archive.bom | tee -a build.log

echo "---- Packing up ----"
freeze gnuplot.packproj

echo "---- Creating DMG ----"
cp $DISTRO_DIR/ReadMe.rtf $DMG_ROOT/.
hdiutil create -volname Gnuplot-$VERSION -fs HFS+ -srcfolder $DMG_ROOT $BUILD_DIR/Gnuplot-$VERSION$BETA.dmg

echo "---- Done ----"