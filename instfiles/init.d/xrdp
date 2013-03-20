#! /bin/sh
#
# start/stop xrdp and sesman daemons

### BEGIN INIT INFO
# Provides:          xrdp
# Required-Start:    $network $remote_fs
# Required-Stop:     $network $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: start xrdp daemon
### END INIT INFO

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/xrdp
PIDDIR=/var/run/xrdp
USERID=xrdp
RSAKEYS=/etc/xrdp/rsakeys.ini
NAME=xrdp
DESC=xrdp

test -x $DAEMON || exit 0

if [ -r /etc/default/$NAME ]; then
   . /etc/default/$NAME
fi

# Check for pid dir
if [ ! -d $PIDDIR ] ; then
        mkdir $PIDDIR
fi
chown $USERID:$USERID $PIDDIR

# Check for rsa key 
if [ ! -f $RSAKEYS ] || cmp $RSAKEYS /usr/share/doc/xrdp/rsakeys.ini > /dev/null; then
        echo "Generating xrdp RSA keys..."
        (umask 077 ; xrdp-keygen xrdp $RSAKEYS)
        chown $USERID:$USERID $RSAKEYS
fi


set -e

case "$1" in
  start)
	echo -n "Starting $DESC: "
        start-stop-daemon --start --quiet --oknodo  --pidfile $PIDDIR/$NAME.pid \
	    --chuid $USERID:$USERID --exec $DAEMON
	echo -n "$NAME"
	[ "$SESMAN_START" = "yes" ] && { \
            start-stop-daemon --start --quiet --oknodo --pidfile $PIDDIR/xrdp-sesman.pid \
	       --exec /usr/sbin/xrdp-sesman
	    echo -n " sesman"
	}
	echo "."
	;;
  stop)
	[ -n "$XRDP_UPGRADE" -a "$RESTART_ON_UPGRADE" = "no" ] && {
	    echo "Upgrade in progress, no restart of xrdp."
	    exit 0
	}
	echo -n "Stopping $DESC: "
        start-stop-daemon --stop --quiet --oknodo --pidfile $PIDDIR/xrdp-sesman.pid \
	    --chuid $USERID:$USERID --exec /usr/sbin/xrdp-sesman
	echo -n "sesman "
	start-stop-daemon --stop --quiet --oknodo --pidfile $PIDDIR/$NAME.pid \
	    --exec $DAEMON
	sleep 1
	echo "$NAME."
	;;
  restart)
	$0 stop
	$0 start
	;;
  *)
	N=/etc/init.d/$NAME
	# echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
