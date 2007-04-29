#!/bin/sh
# xrdp control script
# same as xrdp_control.sh except the XRDP_DIR is /usr/lib/xrdp
# Written : 1-13-2006 - Mark Balliet - posicat@pobox.com
# maintaned by Jay Sorg
# chkconfig: 2345 11 89
# description: starts xrdp

XRDP=xrdp
SESMAN=sesman
STARTWM=startwm.sh
XRDP_DIR=/usr/lib/xrdp/
LOG=/dev/null

cd $XRDP_DIR

if ! test -x $XRDP
then
  echo "$XRDP is not executable"
  exit 0
fi
if ! test -x $SESMAN
then
  echo "$SESMAN is not executable"
  exit 0
fi
if ! test -x $STARTWM
then
  echo "$STARTWM is not executable"
  exit 0
fi

xrdp_start()
{
  echo -n "Starting: xrdp and sesman . . "
  ./$XRDP >> $LOG
  ./$SESMAN >> $LOG
  echo "."
  sleep 1
  return 0;
}

xrdp_stop()
{
  echo -n "Stopping: xrdp and sesman . . "
  ./$SESMAN --kill >> $LOG
  ./$XRDP --kill >> $LOG
  echo "."
  return 0;
}

is_xrdp_running()
{
  ps u --noheading -C $XRDP | grep -q -i $XRDP
  if test $? -eq 0
  then
    return 1;
  else
    return 0;
  fi
}

is_sesman_running()
{
  ps u --noheading -C $SESMAN | grep -q -i $SESMAN
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
    if test -e /var/run/sesman.pid
    then
      rm /var/run/sesman.pid
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
    echo "Usage: xrdp_control.sh {start|stop|restart|force-reload}"
    exit 1
esac

exit 0
