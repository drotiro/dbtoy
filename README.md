DBToy: Mapping Relational Databases to File Systems
===================================================

What is dbtoy?
--------------

It's a Linux filesystem, implemented using fuse.
Its purpose is to expose the contents of a relational database
through a set of directories and files.


Compiling dbtoy
---------------

To succesfully compile and run dbtoy you need fuse (2.6 and above)
and the client libraries for the dbms you need to connect to.
Moreover, dbtoy needs libapp for command line parsing, you can fork
or donwload it from [GitHub](http://github.com/drotiro/libapp).
This version supports connections to MySQL and PostgreSQL databases.
There is a "driver" to talk to each dbms, you can enable or disable
every driver at compile time. You should enable at least one driver.

The quickest way build dbtoy is the script 'makeall.sh', included in
the source distribution. This script looks for the needed libraries
and enable the corrisponding drivers.
If you want to choose yourself the drivers to be built, you can pass
the appropriate parameters to make.

For instance, to build both drivers run:

    make MYSQL_DRV=1 POSTGRESQL_DRV=1

To get the correct flags, the Makefile uses the scripts provided by the
dbms developers ('mysql_config' and 'pg_config' respectively) so these
scripts should be in your $PATH.


Playing with dbtoy
------------------

1. First of all, make sure that you can succesfully connect to the
database. 
2. Modprobe fuse kernel module.
3. Run dbtoy passing your credentials and the mount point.

To see a list of options run dbtoy without arguments.

Here's some example:

Connecting to MySQL running on a remote server:

    dbtoy -u username -p password -h remotehost -d mysql /mnt/dbtoy

Connecting to PostgreSQL running locally:

    dbtoy -u postgres -p '' -d postgresql -i postgres /mnt/dbtoy

Then, walk through your filesystem. Each schema has its own directory, 
containing one subdirectory for every table.
Inside the dirs there is one file with the description of the datatypes
and another with the data. Ex.:

* mountpoint
    * schema 1
    *  ...
    * schema N
        * table 1
            * data
            * types
        *  ...
        * table M
            * data
            * types

You can "cat mountpoint/schemaN/tableM/types" to see a description of the table,
or look at the data.
The file format is XML, with the tag names matching those of the corresponding
columns of the database. A custom stylesheet can be added to all files using
the "-x" command line option, e.g:

    dbtoy -u root -p root -x '<?xml-stylesheet type="text/xsl" href="my-style.xsl"?>' /mntpoint

If you feel lucky you can play with the EXPERIMENTAL query file feature:
`"cat data?col=val"` where 'col' is a valid column name and 'val' is a feasible
value for col (and strings must be quoted!)

Copyright and license
---------------------

DBToy is Copyright (C) 2003 - 2010 Domenico Rotiroti.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.
    A copy of the GNU General Public License is included in the source
    distribution of this software.
