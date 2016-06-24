<!-- Use "pandoc -sS -o gdplogd-admin.html gdplogd-admin.md" to process this to HTML -->

# ADMINISTERING THE GLOBAL DATA PLANE LOG SERVER

_Fill in something here_

***Note: this is just a first draft;
these instructions will certainly change.***

## Administrative Statistics Output

Gdplogd can optionally output statistics information
for use by administrative tools and visualizations.
These are formated as text lines in the form:

	timestamp message-id n1=v1; n2=v2 ...

The message-id is a short string used to identify the semantics
of this statistic.
This is optionally followed by a semicolon-separated list of
[name=]value pairs (the name is optional).
All names and values are encoded to avoid ambiguity.
For example, if a name or value contains `=` or `;`
that character will be encoded as `+3d` or `+3b`
respectively.

The messages are from the following list:

`log-create`
  :   Posted when logs are created.  Parameters are:

    * `log-name` --- the name of the log to be created.
    * `status` --- the status of the creation request.

`log-open`
  :   Posted when logs are opened.  Parameters are:

    * `log-name` --- the name of the log to be opened.
    * `status` --- the status of the open request.

`log-snapshot`
  :   Posted periodically as controlled by the
      `swarm.gdplogd.admin.probeintvl` parameter.
      Parameters are:

    * `name` --- the name of the log.
    * `in-cache` --- whether the log is in the in-memory cache.
    * `nrecs` --- the number of records in the log.  Only shown
      if the log is in the cache.
    * `size` --- the size of the on-disk extent files for the log.
      Only the extents currently open are included.
