#!/bin/sh
# xrdp control script
# Written : 1-13-2006 - Mark Balliet - posicat@pobox.com
# maintaned by Jay Sorg
# chkconfig: 2345 11 89
# description: starts xrdp

### BEGIN INIT INFO
# Provides: xrdp
# Required-Start:
# Required-Stop:
# Should-Start:
# Should-Stop:
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: Start and stop xrdp
# Description: starts xrdp
### END INIT INFO

SBINDIR=/usr/local/sbin
LOG=/dev/null
CFGDIR=/etc/xrdp

if ! test -x $SBINDIR/xrdp
then
  echo "xrdp is not executable"
  exit 0
fi
if ! test -x $SBINDIR/xrdp-sesman
then
  echo "xrdp-sesman is not executable"
  exit 0
fi
if ! test -x $CFGDIR/startwm.sh
then
  echo "startwm.sh is not executable"
  exit 0
fi

xrdp_start()
{
  echo -n "Starting: xrdp and sesman . . "
  $SBINDIR/xrdp >> $LOG
  $SBINDIR/xrdp-sesman >> $LOG
  echo "."
  sleep 1
  return 0;
}

xrdp_stop()
{
  echo -n "Stopping: xrdp and sesman . . "
  $SBINDIR/xrdp-sesman --kill >> $LOG
  $SBINDIR/xrdp --kill >> $LOG
  echo "."
  return 0;
}

is_xrdp_running()
{
  ps u --noheading -C xrdp | grep -q -i xrdp
  if test $? -eq 0
  then
    return 1;
  else
    return 0;
  fi
}

is_sesman_running()
{
  ps u --noheading -C xrdp-sesman | grep -q -i xrdp-sesman
  if test $? -eq 0
  then
    return 1;
  else
    return 0;
  fi
}

check_up()
{
  # Cleanup : If sesman isn't running, but the pid exists, erase it.
  is_sesman_running
  if test $? -eq 0
  then
    if test -e /var/run/xrdp-sesman.pid
    then
      rm /var/run/xrdp-sesman.pid
    fi
  fi
  # Cleanup : If xrdp isn't running, but the pid exists, erase it.
  is_xrdp_running
  if test $? -eq 0
  then
    if test -e /var/run/xrdp.pid
    then
      rm /var/run/xrdp.pid
    fi
  fi
  return 0;
}

case "$1" in
  start)
    check_up
    is_xrdp_running
    if ! test $? -eq 0
    then
      echo "xrdp is already loaded"
      exit 1
    fi
    is_sesman_running
    if ! test $? -eq 0
    then
      echo "sesman is already loaded"
      exit 1
    fi
    xrdp_start
    ;;
  stop)
    check_up
    is_xrdp_running
    if test $? -eq 0
    then
      echo "xrdp is not loaded."
    fi
    is_sesman_running
    if test $? -eq 0
    then
      echo "sesman is not loaded."
    fi
    xrdp_stop
    ;;
  force-reload|restart)
    check_up
    echo "Restarting xrdp ..."
    xrdp_stop
    is_xrdp_running
    while ! test $? -eq 0
    do
      check_up
      sleep 1
      is_xrdp_running
    done
    xrdp_start
    ;;
  *)
    echo "Usage: xrdp.sh {start|stop|restart|force-reload}"
    exit 1
esac

exit 0
