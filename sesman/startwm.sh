#!/usr/bin/env bash
#
# This script is an example. You might need to edit this script
# depending on your distro if it doesn't work for you.
#
# Uncomment the following line for debug:
# exec xterm


# Execution sequence for interactive login shell - pseudocode
#
# IF /etc/profile is readable THEN
#     execute /etc/profile
# END IF
# IF ~/.bash_profile is readable THEN
#     execute ~/.bash_profile
# ELSE
#     IF ~/.bash_login is readable THEN
#         execute ~/.bash_login
#     ELSE
#         IF ~/.profile is readable THEN
#             execute ~/.profile
#         END IF
#     END IF
# END IF

error_exit() {
    echo "Error: $1" >&2
    exit 1
}

log() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $1" >&2
}

source_if_exists() {
    if [ -r "$1" ]; then
        . "$1" || error_exit "Failed to source $1"
    fi
}


pre_start() {
    source_if_exists /etc/profile
    if [ -r "$HOME/.bash_profile" ]; then
        source_if_exists "$HOME/.bash_profile"
    elif [ -r "$HOME/.bash_login" ]; then
        source_if_exists "$HOME/.bash_login"
    else
        source_if_exists "$HOME/.profile"
    fi
}

# When logging out from the interactive shell, the execution sequence is:
#
# IF ~/.bash_logout exists THEN
#     execute ~/.bash_logout
# END IF
post_start() {
    source_if_exists "$HOME/.bash_logout"
}


  # If DESKTOP_SESSION is set and valid then the STARTUP command will be taken from there
  # GDM exports environment variables XDG_CURRENT_DESKTOP and XDG_SESSION_DESKTOP.
  # This follows it.
  
get_xdg_session_startupcmd() {
    if [ -n "$1" ] && [ -d /usr/share/xsessions ] && [ -f "/usr/share/xsessions/$1.desktop" ]; then
        STARTUP=$(grep '^Exec=' "/usr/share/xsessions/$1.desktop" | cut -d= -f2-)
        XDG_CURRENT_DESKTOP=$(grep '^DesktopNames=' "/usr/share/xsessions/$1.desktop" | cut -d= -f2- | tr ';' ':')
        export XDG_CURRENT_DESKTOP
        export XDG_SESSION_DESKTOP="$DESKTOP_SESSION"
    fi
}

#start the window manager
wm_start() {
    if [ -r /etc/default/locale ]; then
        . /etc/default/locale || error_exit "Failed to source /etc/default/locale"
        export LANG LANGUAGE
    fi

    if [ -r /etc/X11/Xsession ]; then
        pre_start
        if [ -z "${STARTUP:-}" ] && [ -n "${DESKTOP_SESSION:-}" ]; then
            get_xdg_session_startupcmd "$DESKTOP_SESSION"
        fi
        . /etc/X11/Xsession || error_exit "Failed to source /etc/X11/Xsession"
        post_start
        exit 0
    fi

    if [ -f /etc/alpine-release ]; then
        if [ -f /etc/X11/xinit/xinitrc ]; then
            pre_start
            /etc/X11/xinit/xinitrc || error_exit "Failed to execute /etc/X11/xinit/xinitrc"
            post_start
        else
            error_exit "xinit package isn't installed"
        fi
    fi

    if [ -r /etc/X11/xinit/Xsession ]; then
        pre_start
        . /etc/X11/xinit/Xsession || error_exit "Failed to source /etc/X11/xinit/Xsession"
        post_start
        exit 0
    fi

    if [ -r /etc/X11/xdm/Xsession ]; then
        . /etc/X11/xdm/Xsession || error_exit "Failed to source /etc/X11/xdm/Xsession"
        exit 0
    elif [ -r /usr/etc/X11/xdm/Xsession ]; then
        . /usr/etc/X11/xdm/Xsession || error_exit "Failed to source /usr/etc/X11/xdm/Xsession"
        exit 0
    fi

    pre_start
    xterm || error_exit "Failed to start xterm"
    post_start
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

log "Starting window manager"
wm_start
error_exit "Window manager exited unexpectedly"
