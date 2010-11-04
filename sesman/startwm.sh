#!/bin/sh

# change the order in line below to run to run whatever window manager you
# want, default to kde

SESSIONS="gnome-session blackbox fluxbox startxfce4 startkde xterm"

#start the window manager
wm_start()
{
  for WindowManager in $SESSIONS
  do
    which $WindowManager
    if test $? -eq 0
    then
      echo "Starting $WindowManager"
      $WindowManager
      return 0
    fi
  done
  return 0
}

#Execution sequence for interactive login shell
#Following pseudo code explains the sequence of execution of these files.
#execute /etc/profile
#IF ~/.bash_profile exists THEN
#    execute ~/.bash_profile
#ELSE
#    IF ~/.bash_login exist THEN
#        execute ~/.bash_login
#    ELSE
#        IF ~/.profile exist THEN
#            execute ~/.profile
#        END IF
#    END IF
#END IF
pre_start()
{
  if [ -f /etc/profile ]
  then
    . /etc/profile
  fi
  if [ -f ~/.bash_profile ]
  then
    . ~/.bash_profile
  else
    if [ -f ~/.bash_login ]
    then
      . ~/.bash_login
    else
      if [ -f ~/.profile ]
      then
        . ~/.profile
      fi
    fi
  fi
  return 0
}

#When you logout of the interactive shell, following is the
#sequence of execution:
#IF ~/.bash_logout exists THEN
#    execute ~/.bash_logout
#END IF
post_start()
{
  if [ -f ~/.bash_logout ]
  then
    . ~/.bash_logout
  fi
  return 0
}

#. /etc/environment
#export PATH=$PATH
#export LANG=$LANG

# change PATH to be what your environment needs usually what is in
# /etc/environment
#PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games"
#export PATH=$PATH

# for PATH and LANG from /etc/environment
# pam will auto process the environment file if /etc/pam.d/xrdp-sesman
# includes
# auth       required     pam_env.so readenv=1

pre_start
wm_start
post_start

exit 1
