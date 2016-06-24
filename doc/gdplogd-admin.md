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

### Example

This shows the output from one log open and two snapshots.

    2016-06-24T20:44:18.673694000Z log-open log-name=u6Uy1qETHk2ntODficWLFgNmoeKDly-qk6yQN1EcZow; status=OK
    2016-06-24T20:44:28.675471000Z log-snapshot name=K5d008wsNPKURynbeh5koBMLXfZHt0iC_-VZqoLqpvA; in-cache=false
    2016-06-24T20:44:28.675931000Z log-snapshot name=b5IEO6R2DM8S0cikYtB24Cqeyt0B9eatyLnn2qCl5WQ; in-cache=false
    2016-06-24T20:44:28.676431000Z log-snapshot name=u6Uy1qETHk2ntODficWLFgNmoeKDly-qk6yQN1EcZow; in-cache=true; nrecs=2; size=0
    2016-06-24T20:44:28.676555000Z log-snapshot name=zf-Jke7aELKuoS6vYT60XPnWhrxcGWImHEFzyU-Dyf0; in-cache=false
    2016-06-24T20:44:38.673993000Z log-snapshot name=K5d008wsNPKURynbeh5koBMLXfZHt0iC_-VZqoLqpvA; in-cache=false
    2016-06-24T20:44:38.674336000Z log-snapshot name=b5IEO6R2DM8S0cikYtB24Cqeyt0B9eatyLnn2qCl5WQ; in-cache=false
    2016-06-24T20:44:38.674702000Z log-snapshot name=u6Uy1qETHk2ntODficWLFgNmoeKDly-qk6yQN1EcZow; in-cache=true; nrecs=2; size=0
    2016-06-24T20:44:38.674815000Z log-snapshot name=zf-Jke7aELKuoS6vYT60XPnWhrxcGWImHEFzyU-Dyf0; in-cache=false
