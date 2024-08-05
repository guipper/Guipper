# Attempt to load a config.make file.
# If none is found, project defaults in config.project.make will be used.
ifneq ($(wildcard config.make),)
	include config.make
endif

# make sure the the OF_ROOT location is defined
ifndef OF_ROOT
	OF_ROOT=$(realpath ../../..)
endif

# call the project makefile!
include $(OF_ROOT)/libs/openFrameworksCompiled/project/makefileCommon/compile.project.mk

PROJECT_NAME := $(notdir $(CURDIR))
UNAME_S := $(shell uname -s)

# Post-build step to copy the data folder
ifeq ($(UNAME_S), Darwin)
post-build:
	cp -r bin/data bin/$(PROJECT_NAME).app/Contents/Resources/
else
post-build:
	@echo "Post-build step skipped for non-macOS platforms."
endif

# Add the post-build step to the default target
all: $(OF_TARGET) post-build
