# Run this script on a clean source checkout to get ready for building.

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
olddir=`pwd`

cd $srcdir

# Checkout and update submodules
if test -d .git; then
    git submodule init .
    git submodule update --recursive
fi

# NOCONFIGURE is used by gnome-common
if test -z "$NOCONFIGURE"; then
    echo "This script will run ./configure automatically. If you wish to pass "
    echo "any arguments to it, please specify them on the $0 "
    echo "command line. To disable this behavior, have NOCONFIGURE=1 in your "
    echo "environment."
fi

# Run the actual tools to prepare the clean checkout
mkdir -p m4
autoreconf -fi -I m4 || exit $?

cd "$olddir"
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
