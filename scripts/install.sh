#!/bin/sh
# Installs webserv as a system service: the binary into
# /usr/local/sbin, config into /etc/webserv, document root into
# /var/www/webserv, logs into /var/log/webserv, and a systemd unit.
# Needs root. Re-running this (an upgrade) never overwrites an
# already-installed config or site - only a first install populates
# those, so editing /etc/webserv/webserv.conf after install is safe
# across upgrades.
#
# Usage: sudo ./scripts/install.sh

set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "install.sh must be run as root (sudo ./scripts/install.sh)" >&2
    exit 1
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)

BIN_DEST=/usr/local/sbin/webserv
CONF_DIR=/etc/webserv
WWW_DIR=/var/www/webserv
LOG_DIR=/var/log/webserv
UNIT_DEST=/etc/systemd/system/webserv.service

if [ ! -x "$ROOT_DIR/webserv" ]; then
    echo "No built binary found, building..."
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build"
    cmake --build "$ROOT_DIR/build" -j
fi

echo "Installing binary to $BIN_DEST"
install -m 755 "$ROOT_DIR/webserv" "$BIN_DEST"

echo "Creating $CONF_DIR, $WWW_DIR, $LOG_DIR"
mkdir -p "$CONF_DIR" "$WWW_DIR" "$LOG_DIR"

if [ ! -f "$CONF_DIR/mime.types" ]; then
    cp "$ROOT_DIR/conf/mime.types" "$CONF_DIR/mime.types"
    echo "Installed $CONF_DIR/mime.types"
fi

if [ ! -f "$CONF_DIR/webserv.conf" ]; then
    cp "$ROOT_DIR/conf/webserv.conf.install" "$CONF_DIR/webserv.conf"
    echo "Installed $CONF_DIR/webserv.conf"
else
    echo "$CONF_DIR/webserv.conf already exists, leaving it alone"
fi

if [ ! -e "$WWW_DIR/index.html" ]; then
    cp -r "$ROOT_DIR/WWW/." "$WWW_DIR/"
    echo "Installed the example site at $WWW_DIR"
else
    echo "$WWW_DIR already has content, leaving it alone"
fi

echo "Installing systemd unit to $UNIT_DEST"
cp "$ROOT_DIR/scripts/webserv.service" "$UNIT_DEST"
systemctl daemon-reload 2>/dev/null || echo "Note: systemctl daemon-reload failed (no systemd?) - the unit file is in place, run it yourself once systemd is available"

echo
echo "Done. To start it:"
echo "  systemctl start webserv"
echo "  systemctl enable webserv    # start on boot"
echo "  systemctl status webserv"
echo "  journalctl -u webserv -f"
echo
echo "Config: $CONF_DIR/webserv.conf"
echo "Site:   $WWW_DIR"
echo "Logs:   $LOG_DIR"
