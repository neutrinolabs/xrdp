#!/bin/bash
# Builds a .deb of this source tree, with wlup, to try it on a Debian or
# Ubuntu host and remove it cleanly again (apt purge xrdp-wlup). It is a
# quick test package, not a replacement for the distribution's xrdp.
#
# Run it from an empty build directory, like configure:
#
#   mkdir ../xrdp-deb && cd ../xrdp-deb && ../xrdp/wlup/make-deb.sh
#   sudo apt install ./xrdp-wlup_*.deb
#
# The package uses the standard layout and conflicts with the xrdp package:
#
#   /usr/sbin, /usr/bin, /usr/libexec/xrdp, /usr/lib/xrdp   programs, libs
#   /etc/xrdp, /etc/pam.d/xrdp-sesman                       conffiles
#   /usr/lib/systemd/system/xrdp*.service                   not enabled
#
# GNOME (code=31, backend=mutter) is the default session. The services are
# not started; start them with: systemctl start xrdp-sesman xrdp
#
# Needs the build dependencies of xrdp and wlup, plus libx264-dev and
# dpkg-dev. Uses ccache when it is installed.
set -euo pipefail
# the package keeps the staged modes: no group-writable /etc/xrdp
umask 022

SRC=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$(pwd)
if [ "$BUILD" = "$SRC" ]; then
    echo "run $0 from a separate build directory" >&2
    exit 1
fi
PKG=xrdp-wlup
SHA=$(git -C "$SRC" rev-parse --short HEAD)
DATE=$(git -C "$SRC" log -1 --format=%cd --date=format:%Y%m%d)
UPSTREAM=$(sed -n 's/^AC_INIT(\[xrdp\], \[\([^]]*\)\].*/\1/p' "$SRC/configure.ac")
VERSION="${UPSTREAM}~wlup${DATE}.git${SHA}"
if [ -n "$(git -C "$SRC" status --porcelain --untracked-files=no)" ]; then
    VERSION="${VERSION}.dirty"
fi
ARCH=$(dpkg --print-architecture)
ROOT=$BUILD/pkgroot
MAINTAINER="${DEBFULLNAME:-$(git -C "$SRC" config user.name || echo xrdp-wlup)}"
MAINTAINER="$MAINTAINER <${DEBEMAIL:-$(git -C "$SRC" config user.email || echo root@localhost)}>"
CC="gcc"
if command -v ccache > /dev/null; then
    CC="ccache gcc"
fi

echo "== configure ($VERSION)"
(cd "$SRC" && ./bootstrap > "$BUILD/bootstrap.log" 2>&1)
# -Wno-error=nonnull: GCC 16 warns about xrdp/xrdp_mm.c, outside wlup
"$SRC/configure" --enable-wlup --enable-wlup-mutter --enable-fuse --enable-pixman \
    --enable-opus --enable-jpeg --enable-ipv6 --enable-x264 \
    --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
    CC="$CC" CFLAGS="-O2 -g -Wno-error=nonnull" > configure.log 2>&1

# make does not rebuild objects when only the -D path flags change, so
# always start from clean objects (ccache keeps this cheap)
echo "== build"
make clean > /dev/null 2>&1 || true
make -j"$(nproc)" > make.log 2>&1

echo "== stage"
rm -rf "$ROOT"
make install DESTDIR="$ROOT" > install.log 2>&1
rm -rf "$ROOT/usr/include" "$ROOT/usr/lib/pkgconfig"
find "$ROOT" \( -name '*.la' -o -name '*.a' \) -delete

ETC=$ROOT/etc/xrdp
# keys are made per host in postinst, never shipped in the package
rm -f "$ETC/rsakeys.ini" "$ETC/cert.pem" "$ETC/key.pem"

# GNOME is the default session both ways: autorun= for clients that send
# a username and password, and first in the list for the login screen,
# whose session dropdown starts at the first session section
GNOME_INI=$(mktemp)
cat > "$GNOME_INI" <<'EOF'
; Added by the xrdp-wlup test package (same as wlup/test/mutter)
[GNOME]
name=GNOME (wlup prototype)
lib=libwlup.so
username=ask
password=ask
port=-1
code=31
backend=mutter

EOF
awk -v f="$GNOME_INI" '
    /^\[Xorg\]/ && !done { while ((getline l < f) > 0) print l; done = 1 }
    { print }
    END { if (!done) exit 1 }' "$ETC/xrdp.ini" > "$ETC/xrdp.ini.new"
rm -f "$GNOME_INI"
mv "$ETC/xrdp.ini.new" "$ETC/xrdp.ini"
sed -i 's/^autorun=.*/autorun=GNOME/' "$ETC/xrdp.ini"

echo "== control files"
mkdir -p "$ROOT/DEBIAN"
(cd "$ROOT" && find etc -type f | sed 's|^|/|' | sort) > "$ROOT/DEBIAN/conffiles"

# shared-library dependencies of the binaries (the private libs live in
# the package itself, under /usr/lib/xrdp)
mkdir -p shlibs/debian
printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$PKG" "$PKG" > shlibs/debian/control
mapfile -t ELF < <(find "$ROOT/usr" -type f \( -perm -u+x -o -name '*.so*' \) \
    -exec sh -c 'head -c4 "$1" | grep -q ELF' _ {} \; -print)
DEPS=$(cd shlibs && dpkg-shlibdeps -O -l"$ROOT/usr/lib/xrdp" "${ELF[@]}" 2>shlibdeps.log |
       sed -n 's/^shlibs:Depends=//p')

cat > "$ROOT/DEBIAN/control" <<EOF
Package: $PKG
Version: $VERSION
Architecture: $ARCH
Maintainer: $MAINTAINER
Depends: $DEPS, openssl
Recommends: gnome-session, gnome-shell
Suggests: pipewire-module-xrdp
Conflicts: xrdp
Replaces: xrdp
Provides: xrdp
Section: net
Priority: optional
Installed-Size: $(du -sk --exclude=DEBIAN "$ROOT" | cut -f1)
Description: xrdp with the wlup Wayland/GNOME backend (test build)
 Test build of xrdp with wlup, from commit $SHA.
 Services are not enabled on install; start them with
 systemctl start xrdp-sesman xrdp.
EOF

cat > "$ROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
ETC=/etc/xrdp
if [ "$1" = configure ]; then
    [ -f $ETC/rsakeys.ini ] || xrdp-keygen xrdp $ETC/rsakeys.ini > /dev/null
    chmod 600 $ETC/rsakeys.ini
    if [ ! -f $ETC/cert.pem ]; then
        openssl req -x509 -newkey rsa:2048 -sha256 -nodes -days 365 \
            -subj "/CN=$(hostname -f 2>/dev/null || hostname)" \
            -keyout $ETC/key.pem -out $ETC/cert.pem > /dev/null 2>&1
        chmod 600 $ETC/key.pem
    fi
    ldconfig
    [ -d /run/systemd/system ] && systemctl daemon-reload || true
fi
exit 0
EOF

cat > "$ROOT/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if [ -d /run/systemd/system ]; then
    systemctl stop xrdp.service xrdp-sesman.service > /dev/null 2>&1 || true
fi
exit 0
EOF

cat > "$ROOT/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = purge ]; then
    rm -f /etc/xrdp/rsakeys.ini /etc/xrdp/cert.pem /etc/xrdp/key.pem \
          /var/log/xrdp.log /var/log/xrdp-sesman.log
    rmdir --ignore-fail-on-non-empty /etc/xrdp/pulse /etc/xrdp 2>/dev/null || true
fi
ldconfig
[ -d /run/systemd/system ] && systemctl daemon-reload || true
exit 0
EOF
chmod 755 "$ROOT/DEBIAN/postinst" "$ROOT/DEBIAN/prerm" "$ROOT/DEBIAN/postrm"

echo "== package"
DEB="$BUILD/${PKG}_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group -Zxz --build "$ROOT" "$DEB" > /dev/null
echo "$DEB"
