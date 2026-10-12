APP_NAME := pool
TABOS_APP_HEAP_BYTES := 1835008
TABOS_APP_STACK_BYTES := 16384
SOURCES := src/main.c src/table.c src/render.c src/game.c src/input.c src/physics.c src/clock.c src/rules.c src/ai.c src/sound.c

include ../../sdk/make/application.mk

INSTALL_LICENSE_PATH ?= $(PROJECT_ROOT)/.local/rootfs/T/share/licenses/pool

install: install-license
.PHONY: install-license
install-license:
	@mkdir -p "$(INSTALL_LICENSE_PATH)"
	cp LICENSE "$(INSTALL_LICENSE_PATH)/LICENSE"
