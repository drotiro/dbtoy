# Variables
# (customize if needed)
CC=gcc
CLIENT_FLAGS := -g
HERE :=`basename $(PWD)`
FUSE_FLAGS	:= `pkg-config --cflags fuse`
FUSE_LIBS	:= `pkg-config --libs fuse`
OBJS := dbtoy.o largetext.o xml_format.o
DRIVERLIST = ""

ifdef MYSQL_DRV
	CLIENT_FLAGS += -DHAVE_MYSQL_DRV `mysql_config --include`
	CLIENT_LIBS	+= `mysql_config --libs`
	OBJS += dbtoy_mysql.o
	DRIVERLIST += mysql
endif

ifdef POSTGRESQL_DRV
	CLIENT_FLAGS += -DHAVE_POSTGRESQL_DRV -I`pg_config --includedir`
	CLIENT_LIBS	+= `pg_config --ldflags` -lpq
	OBJS += dbtoy_pgsql.o
	DRIVERLIST += postgresql
endif

dbtoy: $(OBJS)
	@echo "Building with the following drivers: $(DRIVERLIST)"
	$(CC) -o dbtoy $(OBJS) $(FUSE_LIBS) $(CLIENT_LIBS)

.c.o:
	$(CC) $(FUSE_FLAGS) $(CLIENT_FLAGS) -c $< -o $@

dbtoy.o dbtoy_mysql.o: dbtoy.h


.PHONY: clean 

clean:
	rm -f *.o dbtoy *~
