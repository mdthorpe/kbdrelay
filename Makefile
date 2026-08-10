# kbdrelay build helpers — thin wrappers around PlatformIO.
#
#   make build ENV=kbd        compile an env (kbd | tgt | mock_kbd | mock_tgt)
#   make upload ENV=tgt       build + flash over USB-C (board in download mode)
#   make monitor              open serial monitor on the USB-UART adapter
#   make test                 run host unit tests (native, no hardware)
#   make reconfigure ENV=kbd  force a clean rebuild of one env
#   make clean                remove all build output
#   make distclean            also drop fetched components + lockfile
#
# Overridable variables:
#   PIO           path to platformio (default: bundled penv)
#   ENV           target environment (default: kbd)
#   MONITOR_PORT  serial port for `monitor` (default: the USB-UART adapter)
#   UPLOAD_PORT   serial port for `upload` (default: PlatformIO auto-detect)

PIO          ?= $(HOME)/.platformio/penv/bin/platformio
ENV          ?= kbd
MONITOR_PORT ?= /dev/cu.usbserial-0001
UPLOAD_PORT  ?=

.DEFAULT_GOAL := help
.PHONY: help build upload monitor test reconfigure clean distclean

help:
	@echo "kbdrelay targets:"
	@echo "  make build ENV=<env>        compile (env: kbd|tgt|mock_kbd|mock_tgt)"
	@echo "  make upload ENV=<env>       build + flash over USB-C"
	@echo "  make monitor [MONITOR_PORT=/dev/cu.xxx]"
	@echo "  make test                   host unit tests (native)"
	@echo "  make reconfigure ENV=<env>  clean rebuild of one env"
	@echo "  make clean | distclean"

build:
	$(PIO) run -e $(ENV)

upload:
	$(PIO) run -e $(ENV) -t upload $(if $(UPLOAD_PORT),--upload-port $(UPLOAD_PORT),)

monitor:
	$(PIO) device monitor -e $(ENV) -p $(MONITOR_PORT)

test:
	$(PIO) test -e native

reconfigure:
	rm -rf .pio/build/$(ENV) sdkconfig.$(ENV)
	$(PIO) run -e $(ENV)

clean:
	rm -rf .pio/build

distclean:
	rm -rf .pio managed_components dependencies.lock
