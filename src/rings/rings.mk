# Shared build rules for Rings and its variants.
#
# A variant folder's Makefile sets TARGET, RINGS_DIR (the folder containing
# this file) and RINGS_DEFS (extra -D flags: model, polyphony, string synth),
# then includes this file.

RINGS_DIR ?= .
EURORACK_DIR = $(RINGS_DIR)/../../lib/eurorack

CPP_SOURCES = \
$(RINGS_DIR)/rings.cpp \
$(RINGS_DIR)/../../lib/loewy.cpp

# The Mutable Instruments sources (.cc in lib/eurorack) are compiled through
# one-line .cpp wrappers in mi/, as libDaisy's Makefile only knows .cpp.
CPP_SOURCES += $(wildcard $(RINGS_DIR)/mi/*.cpp)

C_DEFS += $(RINGS_DEFS)
C_INCLUDES += -I$(EURORACK_DIR)

LIBDAISY_DIR = $(RINGS_DIR)/../../lib/libDaisy
DAISYSP_DIR = $(RINGS_DIR)/../../lib/DaisySP

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
