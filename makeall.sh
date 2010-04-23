#!/bin/bash

DRV=""

which pg_config >/dev/null 2>&1
if [ $? == 0 ] ; then DRV="POSTGRESQL_DRV=1"; fi

which mysql_config >/dev/null 2>&1
if [ $? == 0 ] ; then DRV="$DRV MYSQL_DRV=1"; fi

if [ -z "$DRV" ]; then
	echo "*** Could not find a supported library."
	echo "*** Please install mysql or postgres client libs or update your PATH."
else
	make $DRV
fi
