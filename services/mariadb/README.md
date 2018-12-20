# Setting up MariaDB for the GDP

NOTE WELL: These services do not actually work in a Docker container
for a variety of reasons.  They are merely a clue for the right path.
We'll get them automated as we can, but they were originally designed
to work on a raw system rather than in Docker, and the conversion has
not been completed.

MariaDB is used to store the Human-Oriented Name to GDPname Directory
(HONGD) and may be used for other purposes in the future.  In our
configuration it runs in a Docker container.

This code runs on the host for the Docker container.  It starts up a
Docker container that in turn starts up MariaDB.

We use a publicly supported Docker image (mariadb:10.4).  Everything in
that image should already be set up once we initialize the database.
