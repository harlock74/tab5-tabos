APP_NAME := basic
SOURCES := src/main.c src/basic_engine.c src/basic_kernal.c src/basic_storage.c src/tabos_runtime.c src/basic_extensions.c src/basic_graphics.c src/basic_c64.c src/basic_sid.c src/basic_sound.c upstream/cbmbasic.c
TABOS_APP_HEAP_BYTES := 262144
TABOS_APP_STACK_BYTES := 32768

TABOS_RUNTIME_ASSETS := examples/PONG.prg examples/GRAPHICS.prg examples/AMAZING.prg examples/C64SCREEN.prg examples/C64COLORS.prg examples/C64SPRITE.prg examples/C64ANIM.prg examples/C64DODGE.prg examples/J0SPRITE.prg examples/J0BORDER.prg examples/J0CELL.prg examples/J0FULL.prg examples/SIDTONE.prg examples/SIDSCALE.prg examples/SIDENV.prg examples/SID3VOICE.prg examples/SIDSPRITE.prg

include ../../sdk/make/application.mk

install: install-license
.PHONY: install-license
install-license:
	@mkdir -p ../../.local/rootfs/T/share/licenses/basic
	cp LICENSE UPSTREAM.md examples/THIRD_PARTY.md examples/UNLICENSE ../../.local/rootfs/T/share/licenses/basic/
