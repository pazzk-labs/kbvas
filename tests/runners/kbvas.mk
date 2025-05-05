# SPDX-License-Identifier: MIT

COMPONENT_NAME = KBVAS

SRC_FILES = \
	../kbvas.c \

TEST_SRC_FILES = \
	src/kbvas_test.cpp \
	src/test_all.cpp \
	../external/libmcu/modules/common/src/base64.c \
	../external/libmcu/tests/stubs/logging.cpp \

INCLUDE_DIRS = \
	$(CPPUTEST_HOME)/include \
	../ \
	../external/libmcu/modules/common/include \
	../external/libmcu/modules/logging/include \

MOCKS_SRC_DIRS =
CPPUTEST_CPPFLAGS = -include ../external/libmcu/modules/logging/include/libmcu/logging.h \
		    -DKBVAS_DEBUG=debug \
		    -DKBVAS_INFO=info \
		    -DKBVAS_ERROR=error \

include runners/MakefileRunner
