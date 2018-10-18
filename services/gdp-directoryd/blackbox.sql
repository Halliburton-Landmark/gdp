# ==============================================================================
# Usage: sudo mysql -v --show-warnings < blackbox.sql
# ==============================================================================
#
#	----- BEGIN LICENSE BLOCK -----
#	Applications for the Global Data Plane
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2018, Regents of the University of California.
#	All rights reserved.
#
#	Permission is hereby granted, without written agreement and without
#	license or royalty fees, to use, copy, modify, and distribute this
#	software and its documentation for any purpose, provided that the above
#	copyright notice and the following two paragraphs appear in all copies
#	of this software.
#
#	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#	OR MODIFICATIONS.
#	----- END LICENSE BLOCK -----
#
# directory server expects this database name
#
use blackbox;

# ==============================================================================
# tables
# ==============================================================================

#
# graph and nhops are entangled, so drop any prior table definitions as a unit
#
drop table if exists blackbox.graph;
drop table if exists blackbox.nhops;
drop table if exists blackbox.guids;

#
# blackbox.guids holds guid<->id assignments (oqgraph supports bigint id)
#
create table blackbox.guids
(
	guid BINARY(32) not null,
	id bigint unsigned not null auto_increment,
	primary key (guid),
	key (id)
);

#
# blackbox.nhop holds graph arrows as id pairs (origid -> destid)
#
create table blackbox.nhops
(
	origid bigint unsigned not null,
	destid bigint unsigned not null,
	ts TIMESTAMP,
	primary key (origid, destid),
	key (destid)
);

#
# blackbox.graph is the graph engine viewport into blackbox.nhops state
#
create table blackbox.graph
(
	latch varchar(32) null,
	origid bigint unsigned null,
	destid bigint unsigned null,
	weight double null,
	seq bigint unsigned null,
	linkid bigint unsigned null,
	key (latch, origid, destid) using hash,
	key (latch, destid, origid) using hash
) ENGINE=OQGRAPH data_table='nhops' origid='origid' destid='destid';

# ==============================================================================
# stored procedures
# ==============================================================================

drop procedure if exists blackbox.drop_expired;
delimiter //
create procedure blackbox.drop_expired
(
)
begin
	delete from blackbox.nhops where (ts) < DATE_SUB(NOW(), INTERVAL 5 MINUTE);
	delete from blackbox.guids where id not in (select origid from blackbox.nhops union select destid from blackbox.nhops);
end //
delimiter ;

#
# blackbox.add_nhop inserts a nexthop arrow from eguid to dguid
#
drop procedure if exists blackbox.add_nhop;
delimiter //
create procedure blackbox.add_nhop
(
	IN eguid BINARY(32),
	IN dguid BINARY(32)
)
begin
	set @eid = NULL;
	set @did = NULL;
	#
	# guid id assignment and refresh via insert ignore to avoid renumbering
	#
	insert ignore into blackbox.guids (guid) values (eguid);
	select id into @eid from blackbox.guids where guid = eguid;
	insert ignore into blackbox.guids (guid) values (dguid);
	select id into @did from blackbox.guids where guid = dguid;
	# replace ensures timestamp update if row exists
	replace into blackbox.nhops (origid, destid, ts) values (@eid, @did, CURRENT_TIMESTAMP);
	end //
delimiter ;

#
# blackbox.find_nhop returns nexthop from eguid toward closest dguid instance
#
drop procedure if exists blackbox.find_nhop;
delimiter //
create procedure blackbox.find_nhop
(
	IN eguid BINARY(32),
	IN dguid BINARY(32)
)
begin
	set @oid = NULL;
	set @did = NULL;
	select id into @oid from blackbox.guids where guid = eguid;
	select id into @did from blackbox.guids where guid = dguid;
	#
	select guid from graph inner join guids where latch='dijkstras' and origid = @oid and destid = @did and weight = 1 and seq = 1 and id = linkid limit 1;
	#
end //
delimiter ;

#
# blackbox.mfind_nhop returns nexthop(s) from eguid toward many dguid instances
#
drop procedure if exists blackbox.mfind_nhop;
delimiter //
create procedure blackbox.mfind_nhop
(
	IN eguid BINARY(32),
	IN dguid BINARY(32)
)
begin
	set @oid = NULL;
	set @did = NULL;
	select id into @oid from blackbox.guids where guid = eguid;
	select id into @did from blackbox.guids where guid = dguid;
	#
	select guid from blackbox.graph lasthop inner join blackbox.graph nexthop inner join guids where lasthop.latch='dijkstras' and lasthop.destid = @did and lasthop.weight = 1 and nexthop.latch='dijkstras' and nexthop.origid = @oid and nexthop.destid = lasthop.linkid and nexthop.weight = 1 and nexthop.seq = 1 and @oid != @did and guids.id = nexthop.linkid;
	#
end //
delimiter ;

#
# blackbox.delete_nhop deletes a nexthop arrow from eguid to dguid, if any
#
drop procedure if exists blackbox.delete_nhop;
delimiter //
create procedure blackbox.delete_nhop
(
	IN eguid BINARY(32),
	IN dguid BINARY(32)
)
begin
	set @eid = NULL;
	set @did = NULL;
	select id into @did from blackbox.guids where guid = dguid;
	select id into @eid from blackbox.guids where guid = eguid;
	delete from blackbox.nhops where origid = @eid and destid = @did;
	end //
delimiter ;

#
# blackbox.flush_nhops deletes all nexthop arrows from eguid to any guid, if any
#
drop procedure if exists blackbox.flush_nhops;
delimiter //
create procedure blackbox.flush_nhops
(
	IN eguid BINARY(32)
)
begin
	set @eid = NULL;
	select id into @eid from blackbox.guids where guid = eguid;
	delete from blackbox.nhops where origid = @eid;
	end //
delimiter ;

# ==============================================================================
# installation sanity test (see ./test/README.md for additional tests)
# ==============================================================================

delete from guids;
delete from nhops;

call add_nhop(x'A1', x'A2');
call add_nhop(x'A2', x'A3');
call add_nhop(x'A3', x'A4');
call add_nhop(x'A4', x'A5');
call add_nhop(x'A5', x'A6');

call add_nhop(x'A7', x'A8');
call add_nhop(x'A8', x'A9');
call add_nhop(x'A9', x'B1');
call add_nhop(x'A9', x'B2');
call add_nhop(x'A9', x'B3');

# add shortcut from A2 directly to A5
call add_nhop(x'A2', x'A5');

# add path from A6 to A2 via A5
call add_nhop(x'A6', x'A5');
call add_nhop(x'A5', x'A2');

select * from nhops;
select HEX(guid),guid,id from guids;

call find_nhop(x'A1', x'A6');  # answer is HEX(guid) x'A2' aka \242

call find_nhop(x'A1', x'A6');  # answer is HEX(guid) x'A2' aka \242

call find_nhop(x'A2', x'A6');  # answer is HEX(GUID) x'A5' aka \245

call find_nhop(x'A6', x'A2');  # answer is HEX(GUID) x'A5' aka \245

call find_nhop(x'A1', x'A8');  # answer is the Empty set

call delete_nhop(x'A2', x'A5');
call find_nhop(x'A2', x'A6');  # answer has changed to HEX(GUID) x'A3' aka \243

call find_nhop(x'A7', x'B1');  # answer is HEX(GUID) x'A8' aka \250
call flush_nhops(x'A9');
call find_nhop(x'A7', x'B1');  # answer is the Empty set
call find_nhop(x'A7', x'A9');  # answer is HEX(GUID) x'A8' aka \250

delete from guids;
delete from nhops;

# ==============================================================================
