DBToy: Mapping Relational Databases to File Systems
===================================================

What is DBToy?
--------------

It's a Linux filesystem, implemented using fuse.

Its purpose is to expose the contents of a relational database
through a set of directories and files.


Dependencies
------------

To succesfully compile and run dbtoy you need fuse (2.6 and above)
and the client libraries for the dbms you need to connect to.

Moreover, dbtoy needs libapp for command line parsing, you can fork
or donwload it from [GitHub](http://github.com/drotiro/libapp).

This version supports connections to MySQL and PostgreSQL databases.
There is a "driver" to talk to each dbms, you can enable or disable
every driver at compile time. You should enable at least one driver.


Getting started
---------------

The project [Wiki](http://github.com/drotiro/dbtoy/wiki) on GitHub
and the [User Manual](http://github.com/downloads/drotiro/dbtoy/DBToy_UserManual.pdf)
contain instructions for compiling and using DBToy.

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
