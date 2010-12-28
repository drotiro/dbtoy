# Variables
# (customize if needed)
CC=gcc
CLIENT_FLAGS := -g
DEP_FLAGS	:= $(shell pkg-config --cflags fuse)
DEP_LIBS	:= $(shell pkg-config --libs fuse) -lapp
OBJS := dbtoy.o largetext.o xml_format.o
DRIVERLIST = ""
PREFIX=/usr/local

include Makefile.config

ifdef MYSQL_DRV
	CLIENT_FLAGS += -DHAVE_MYSQL_DRV $(shell mysql_config --include)
	CLIENT_LIBS	+= $(shell mysql_config --libs)
	OBJS += dbtoy_mysql.o
	DRIVERLIST += mysql
endif

ifdef POSTGRESQL_DRV
	CLIENT_FLAGS += -DHAVE_POSTGRESQL_DRV -I$(shell pg_config --includedir)
	CLIENT_LIBS	+= $(shell pg_config --ldflags) -lpq
	OBJS += dbtoy_pgsql.o
	DRIVERLIST += postgresql
endif

dbtoy: $(OBJS)
	@echo "Building with the following drivers: $(DRIVERLIST)"
	$(CC) -o dbtoy $(OBJS) $(DEP_LIBS) $(CLIENT_LIBS)

.c.o:
	@echo Compiling $<
	@$(CC) $(DEP_FLAGS) $(CLIENT_FLAGS) -c $< -o $@

#Dependencies
dbtoy.o: dbtoy.c dbtoy.h xml_format.h largetext.h
dbtoy_mysql.o: dbtoy_mysql.c dbtoy.h largetext.h
dbtoy_pgsql.o: dbtoy_pgsql.c dbtoy.h largetext.h
largetext.o: largetext.c largetext.h
xml_format.o: xml_format.c xml_format.h largetext.h dbtoy.h


.PHONY: clean

clean:
	rm -f *.o dbtoy

install: dbtoy
	install -s dbtoy $(PREFIX)/bin
