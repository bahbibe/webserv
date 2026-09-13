#!/bin/sh
# Removes the webserv systemd service and binary. Prompts before
# touching /etc/webserv or /var/log/webserv - config and logs are
# exactly the kind of thing people expect to survive an uninstall
# unless they say otherwise. /var/www/webserv (the site content) is
# left alone entirely - remove it yourself if you want it gone.
#
# Usage: sudo ./scripts/uninstall.sh

set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "uninstall.sh must be run as root (sudo ./scripts/uninstall.sh)" >&2
    exit 1
fi

BIN_DEST=/usr/local/sbin/webserv
CONF_DIR=/etc/webserv
LOG_DIR=/var/log/webserv
UNIT_DEST=/etc/systemd/system/webserv.service

systemctl stop webserv 2>/dev/null || true
systemctl disable webserv 2>/dev/null || true

if [ -f "$UNIT_DEST" ]; then
    rm -f "$UNIT_DEST"
    systemctl daemon-reload 2>/dev/null || true
    echo "Removed $UNIT_DEST"
fi

if [ -f "$BIN_DEST" ]; then
    rm -f "$BIN_DEST"
    echo "Removed $BIN_DEST"
fi

if [ -d "$CONF_DIR" ]; then
    printf "Remove %s (config)? [y/N] " "$CONF_DIR"
    read -r reply
    case "$reply" in
        [yY]*) rm -rf "$CONF_DIR"; echo "Removed $CONF_DIR" ;;
        *) echo "Left $CONF_DIR in place" ;;
    esac
fi

if [ -d "$LOG_DIR" ]; then
    printf "Remove %s (logs)? [y/N] " "$LOG_DIR"
    read -r reply
    case "$reply" in
        [yY]*) rm -rf "$LOG_DIR"; echo "Removed $LOG_DIR" ;;
        *) echo "Left $LOG_DIR in place" ;;
    esac
fi

echo "Note: /var/www/webserv (the site content) was left untouched."
