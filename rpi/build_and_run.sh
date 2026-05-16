#!/usr/bin/env bash
set -euo pipefail

# Build and run helper for the Raspberry Pi honeypot
# Usage:
#   ./build_and_run.sh            # builds using ../include and runs (may use sudo)
#   BACKEND_IP=192.168.1.50 ./build_and_run.sh  # compile with custom backend IP

SRCDIR="$(dirname "$0")"
cd "$SRCDIR"

UNP_DIR="../include"

echo "Building pi_bait (UNP) using UNP headers in $UNP_DIR"

CFLAGS_EXTRA=""
if [ -n "${BACKEND_IP:-}" ] || [ -n "${BACKEND_PORT:-}" ]; then
  if [ -n "${BACKEND_IP:-}" ]; then
    CFLAGS_EXTRA="$CFLAGS_EXTRA -DBACKEND_IP=\"${BACKEND_IP}\""
  fi
  if [ -n "${BACKEND_PORT:-}" ]; then
    CFLAGS_EXTRA="$CFLAGS_EXTRA -DBACKEND_PORT=\"${BACKEND_PORT}\""
  fi
  echo "Using custom backend defines: $CFLAGS_EXTRA"
  make UNP_DIR="$UNP_DIR" CFLAGS="$CFLAGS_EXTRA"
else
  make UNP_DIR="$UNP_DIR"
fi

if [ ! -x ./bin/pi_bait ]; then
  echo "Build completed but ./bin/pi_bait not found or not executable" >&2
  ls -la ./bin || true
  exit 1
fi

echo "Starting pi_bait (may require sudo to bind ports <1024)"

# options: --restart to stop existing instance, --background to run in background
RESTART=0
BACKGROUND=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    -r|--restart)
      RESTART=1; shift ;;
    -b|--background)
      BACKGROUND=1; shift ;;
    --no-sudo)
      NO_SUDO=1; shift ;;
    *)
      shift ;;
  esac
done

# detect running instances
running_pids=$(pgrep -f "bin/pi_bait" || true)
if [ -n "$running_pids" ]; then
  echo "Detected running pi_bait PIDs: $running_pids"
  if [ "$RESTART" -eq 1 ]; then
    echo "Stopping running instance(s)..."
    sudo kill $running_pids || true
    sleep 1
  else
    echo "pi_bait is already running. Use --restart to restart or --background to start another instance." 
    exit 0
  fi
fi

if [ "$BACKGROUND" -eq 1 ]; then
  echo "Starting in background (nohup). Logs: ./pi_bait.log"
  if [ "$(id -u)" -ne 0 ] && [ -z "${NO_SUDO:-}" ]; then
    nohup sudo ./bin/pi_bait > ./pi_bait.log 2>&1 &
  else
    nohup ./bin/pi_bait > ./pi_bait.log 2>&1 &
  fi
  echo $! > ./pi_bait.pid
  echo "Started pid $(cat ./pi_bait.pid)"
  exit 0
fi

# foreground run
if [ "$(id -u)" -ne 0 ] && [ -z "${NO_SUDO:-}" ]; then
  sudo ./bin/pi_bait
else
  ./bin/pi_bait
fi
