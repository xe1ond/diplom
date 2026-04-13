AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = zigbee_stack

zigbee_stack_SOURCES = \
    main.c    \
    phy.c     \
    mac.c     \
    nwk.c     \
    aps.c     \
    streebog.c

AM_CPPFLAGS = \
    -I$(top_srcdir)/include

zigbee_stack_LDADD = \
    -lssl \
    -lcrypto \
    -lgost
