ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

CPPFLAGS += -D_QNX_SOURCE=1
CFLAGS += -D_QNX_SOURCE=1

include $(MKFILES_ROOT)/autotools.mk
include $(MKFILES_ROOT)/qtargets.mk