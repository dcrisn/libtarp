# Recursive top-level Makefile to build subprojects; 
# This Makefile defines various reference path variables
# and exports them to any child makefiles recursively 
# invoked;
#
PROJECT_ROOT      := $(shell realpath $(CURDIR))

# emulate target rootfs and sdk staging_dir
STAGING_DIR       := $(PROJECT_ROOT)/staging

# copy headers here to allow for system header (angle bracket) include syntax
STAGING_HEADERS   := $(STAGING_DIR)/usr/include/

# copy libs here to allow linking against
STAGING_LIBS   := $(STAGING_DIR)/usr/lib

# top directory for subprojects
TARP_TOPDIR       := $(PROJECT_ROOT)/tarp

# rudimentary testing facilities
TARP_COHORT_PATH  := $(TARP_TOPDIR)/cohort

# misc data structures
TARP_MODS_PATH    := $(TARP_TOPDIR)/mods

# common helpers and library files
TARP_COMMON_PATH  := $(TARP_TOPDIR)/common

#
# get a list of absolute paths to add to CPP's include path;
# The paths added are:
#   + the STAGING_HEADERS dir (see above) and all its subdirectories;
#   + the cohort path
#   + the common helpers path
# 
INCLUDE_FLAGS   := $(STAGING_HEADERS)/
INCLUDE_FLAGS   += $(TARP_COHORT_PATH)/
INCLUDE_FLAGS   := $(addprefix -I,$(INCLUDE_FLAGS))

CPPFLAGS        += $(INCLUDE_FLAGS)

CFLAGS          += -fPIC

# 
# get a list of data structure sub-projects paths; 
# This top-level Makefile will invoke each subproject's Makefile
# recursively;
# Each subproject must have its Makefile in its top-level directory;
#
MODS_SUBDIRS    := $(shell find $(TARP_MODS_PATH) -mindepth 1 -maxdepth 1 -type d)

#
# Get only the name of each MODS directory (i.e. last path component) to allow
# individual builds at the command line
MODS_TARGETS    := $(foreach mod, $(MODS_SUBDIRS),$(lastword $(subst /, ,$(mod))))

# the shared library containing all data structures
LIBTARP_SO        := libtarp.so
LIBTARP_OBJS      := $(foreach mod, $(MODS_SUBDIRS), $(shell find $(mod)/src -iname "*.o" 2>/dev/null))
LIBTARP_OBJS      += $(shell find $(TARP_COMMON_PATH) -iname "*.o")
$(info lib objs = $(LIB_OBJS))

LIBTARP_UTILS_SO  := libtarp-utils.so

VALGRIND_REPORT_NAME := valgrind.txt

#
# pass VALGRIND=y at the command line to run output tests binary
# through valgrind
VALGRIND := $(filter y,$(VALGRIND))

export PROJECT_ROOT      
export STAGING_DIR       
export STAGING_HEADERS  
export TARP_TOPDIR       
export TARP_COHORT_PATH  
export TARP_MODS_PATH    
export TARP_COMMON_PATH  
export MODS_SUBDIRS
export CPPFLAGS
export CFLAGS
export VALGRIND
export VALGRIND_REPORT_NAME

#$(info TARP_TOPDIR is $(TARP_TOPDIR))
#$(info TARP_MODS_PATH is $(TARP_MODS_PATH))
#$(info TARP_COMMON_PATH is $(TARP_COMMON_PATH))


.PHONY: all mods clean

all: $(LIBTARP_UTILS_SO) mods $(LIBTARP_SO)

mods: prepare $(LIBTARP_UTILS_SO)
	@for mod in $(MODS_TARGETS); do \
		echo; \
		echo "[ ] Building $$mod"; \
		$(MAKE) -C $(TARP_MODS_PATH)/$$mod; \
		done

prepare:
	mkdir -p $(STAGING_HEADERS)
	mkdir -p $(STAGING_LIBS)
	find $(TARP_COMMON_PATH) -ipath "*/include" -exec dirname {} \; | uniq | xargs -I input sh -c "cp -urf input/include/* $(STAGING_HEADERS)"
	find $(TARP_MODS_PATH) -ipath "*/include" -exec dirname {} \; | uniq | xargs -I input sh -c "cp -urf input/include/* $(STAGING_HEADERS)"

clean:
	find $(PROJECT_ROOT) -type f -iname "*.o" -delete
	find $(PROJECT_ROOT) -type f -iname "*.d" -delete
	find $(PROJECT_ROOT) -type d -name 'out' -exec rm -rf {}/* \;
	find $(PROJECT_ROOT) -iname $(VALGRIND_REPORT_NAME) -delete
	rm -rf $(STAGING_DIR)/*

$(foreach mod,$(MODS_TARGETS),$(mod)_clean):
	find $(TARP_MODS_PATH)/$$mod -type f -iname "*.o*" -delete

# build inidividual MOD
$(MODS_TARGETS) : prepare $(LIBTARP_UTILS_SO)
	$(MAKE) -C $(TARP_MODS_PATH)/$@

$(LIBTARP_SO) : mods
	$(CC) $(CFLAGS) $(CPPFLAGS) -shared $(LIBTARP_OBJS) -o $(STAGING_DIR)/usr/lib/$(LIBTARP_SO)

$(LIBTARP_UTILS_SO) : prepare
	$(MAKE) -C $(TARP_COMMON_PATH)

