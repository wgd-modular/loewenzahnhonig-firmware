# Project Name
TARGET = cloudseed

# Sources: this firmware and the module's hardware class. The reverb itself
# comes from the cloudseed-daisy library (lib/cloudseed-daisy), whose
# cloudseed.mk adds its sources, options and rules and includes libDaisy's
# core Makefile.
CPP_SOURCES = \
cloudseed.cpp \
../../lib/loewy.cpp

C_INCLUDES += -I.

# Library Locations
LIBDAISY_DIR = ../../lib/libDaisy
DAISYSP_DIR = ../../lib/DaisySP
CLOUDSEED_DAISY_DIR = ../../lib/cloudseed-daisy

# Board options, see README.md ("Performance"). The library's options
# (CLOUDSEED_MAX_LINES, CLOUDSEED_STAGING, CLOUDSEED_PROFILE, ...) are
# documented in the library's cloudseed.mk and README and can be given on
# the make command line the same way: make CLOUDSEED_PROFILE=1
CLOUDSEED_BOOST ?= 1                 # 480 MHz on silicon revision V or X
CLOUDSEED_SDRAM_WRITE_ALLOCATE ?= 0  # cache SDRAM writes (write-allocate)
CLOUDSEED_SRAM_WRITE_ALLOCATE ?= 0   # internal SRAM: cache writes (write-allocate)
# Experiment: faster SDRAM row/column delays, checked with a memory test at
# boot (off until measured).
CLOUDSEED_SDRAM_FAST_TIMING ?= 0
C_DEFS += -DCLOUDSEED_BOOST=$(strip $(CLOUDSEED_BOOST)) \
-DCLOUDSEED_SDRAM_WRITE_ALLOCATE=$(strip $(CLOUDSEED_SDRAM_WRITE_ALLOCATE)) \
-DCLOUDSEED_SRAM_WRITE_ALLOCATE=$(strip $(CLOUDSEED_SRAM_WRITE_ALLOCATE)) \
-DCLOUDSEED_SDRAM_FAST_TIMING=$(strip $(CLOUDSEED_SDRAM_FAST_TIMING))
# Recorded with the library's options, so that a changed option rebuilds.
CLOUDSEED_EXTRA_CONFIG = $(strip $(CLOUDSEED_BOOST)) \
$(strip $(CLOUDSEED_SDRAM_WRITE_ALLOCATE)) \
$(strip $(CLOUDSEED_SRAM_WRITE_ALLOCATE)) $(strip $(CLOUDSEED_SDRAM_FAST_TIMING))

# This firmware uses no USB host: keep libDaisy's host stack out of the
# flash (the profiling build needs the room).
CLOUDSEED_STRIP_USB_HOST = 1

# The reverb's inner loops are compiled for speed by the library; this file
# (the callback) gets -O2 too, and everything else -Os to fit the Seed's
# 128 KB of flash. The profiling build's logging code needs the flash more
# than the callback and the engine need -O2.
OPT = -Os
CLOUDSEED_PROFILE ?= 0
ifeq ($(strip $(CLOUDSEED_PROFILE)),1)
CLOUDSEED_ENGINE_OPT = -Os
endif

include $(CLOUDSEED_DAISY_DIR)/cloudseed.mk

ifeq ($(strip $(CLOUDSEED_PROFILE)),1)
$(BUILD_DIR)/cloudseed.o: OPT = -Os
else
$(BUILD_DIR)/cloudseed.o: OPT = -O2
endif

# Build, then flash what was built. libDaisy's program-dfu alone flashes
# whatever the build directory holds, and the options are only rebuilt into
# it by a build: `make CLOUDSEED_PROFILE=1 flash`.
.PHONY: flash
flash: all
	$(MAKE) program-dfu
