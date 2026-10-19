#
# BundleCreator.make
#
# Make include file for BundleCreator executable.
#
# This file defines the following make variables:
#
#   BUNDLE_TOOL      Path to the BundleCreator executable.
#   BUNDLE_FLAGS     Common flags passed to BUNDLE_TOOL.
#
# Set BUNDLE_SYMLINKS=1 before including this file to
# preserve symbolic links in bundles (requires system zip).
#

ifndef BUNDLE_TOOL
ifneq (,$(findstring debug,$(DEFAULT_TARGET) $(MAKECMDGOALS)))
BUNDLE_TOOL = $(POCOPLUS_BASE)/OSP/BundleCreator/$(POCO_HOST_BINDIR)/bundled
else
BUNDLE_TOOL = $(POCOPLUS_BASE)/OSP/BundleCreator/$(POCO_HOST_BINDIR)/bundle
endif
endif

BUNDLE_FLAGS ?=
ifdef BUNDLE_SYMLINKS
BUNDLE_FLAGS += -y
endif
