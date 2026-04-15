AM_CFLAGS = -Wall -Wextra -I$(top_srcdir)/include
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS = \
-I$(top_srcdir)/include
bin_PROGRAMS = diplom

diplom_SOURCES = \
main.c    \
srs/phy.c     \
srs/mac.c     \
srs/nwk.c     \
srs/aps.c     \
srs/streebog.c

diplom_LDADD = -lssl -lcrypto

include_HEADERS = \
include/phy.h       \
include/mac.h       \
include/nwk.h       \
include/aps.h       \
include/streebog.h  \
include/aes.h
    
