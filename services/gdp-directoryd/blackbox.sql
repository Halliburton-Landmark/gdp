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
	gdprid bigint unsigned not null,
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
# blackbox.add_nhop inserts a nhop arrow into the graph
#
drop procedure if exists blackbox.add_nhop;
delimiter //
create procedure blackbox.add_nhop
(
	IN eguid BINARY(32),
	IN dguid BINARY(32),
	IN rguid BINARY(32)
)
begin
	set @eid = NULL;
	set @did = NULL;
	set @rid = NULL;
	#
	# guid id assignment and refresh via insert ignore to avoid renumbering
	#
	insert ignore into blackbox.guids (guid) values (eguid);
	select id into @eid from blackbox.guids where guid = eguid;
	insert ignore into blackbox.guids (guid) values (dguid);
	select id into @did from blackbox.guids where guid = dguid;
	insert ignore into blackbox.guids (guid) values (rguid);
	select id into @rid from blackbox.guids where guid = rguid;
	# (re)add @eid -> @did path to graph with latest timestamp
	replace into blackbox.nhops (origid, destid, gdprid, ts) values (@eid, @did, @rid, CURRENT_TIMESTAMP);
	end //
delimiter ;

#
# blackbox.delete_nhop deletes a nhop arrow from the graph
#
drop procedure if exists blackbox.delete_nhop;
delimiter //
create procedure blackbox.delete_nhop
(
	IN eguid BINARY(32),
	IN dguid BINARY(32),
	IN rguid BINARY(32)
)
begin
	set @eid = NULL;
	set @did = NULL;
	set @rid = NULL;
	select id into @did from blackbox.guids where guid = dguid;
	select id into @eid from blackbox.guids where guid = eguid;
	select id into @rid from blackbox.guids where guid = rguid;
	# delete @eid -> @did path from graph
	delete from blackbox.nhops where origid = @eid and destid = @did and gdprid = @rid;
	end //
delimiter ;

#
# blackbox.flush_nhops deletes all nhop arrows owned by rguid from the graph
#
drop procedure if exists blackbox.flush_nhops;
delimiter //
create procedure blackbox.flush_nhops
(
	IN rguid BINARY(32)
)
begin
	set @rid = NULL;
	select id into @rid from blackbox.guids where guid = rguid;
	delete from blackbox.nhops where gdprid = @rid;
	end //
delimiter ;

#
# blackbox.find_nhop returns the best nhop from oguid towards dguid, if any
#
drop procedure if exists blackbox.find_nhop;
delimiter //
create procedure blackbox.find_nhop
(
	IN oguid BINARY(32),
	IN dguid BINARY(32)
)
begin
	declare continue handler for sqlstate '02000' set @nguid = NULL;
	set @oid = NULL;
	set @did = NULL;
	set @eid = NULL;
	select id into @oid from blackbox.guids where guid = oguid;
	select id into @did from blackbox.guids where guid = dguid;
	select linkid into @eid from blackbox.graph where latch='dijkstras' and origid = @oid and destid = @did and seq = 1 limit 1;
    select guid into @nguid from blackbox.guids where id = @eid;
	select @nguid;
end //
delimiter ;

# ==============================================================================
# installation sanity test
# ==============================================================================

delete from guids;
delete from nhops;

# add_nhop(eguid, dguid, rguid)
call add_nhop(x'A1', x'A2', x'F1');
call add_nhop(x'A2', x'A3', x'F1');
call add_nhop(x'A2', x'A5', x'F1');
call add_nhop(x'A3', x'A4', x'F1');
call add_nhop(x'A4', x'A5', x'F1');
call add_nhop(x'A5', x'A6', x'F1');
call add_nhop(x'A7', x'A8', x'F1');
call add_nhop(x'A6', x'A5', x'F1');
call add_nhop(x'A5', x'A2', x'F1');
call add_nhop(x'A8', x'A9', x'F9');
call add_nhop(x'A9', x'B1', x'F9');

select * from nhops;
select HEX(guid),id from guids;

call find_nhop(x'A1', x'A6');  # answer is HEX(guid) x'A2'
select HEX(@nguid);

call find_nhop(x'A2', x'A6');  # answer is HEX(GUID) x'A5'
select HEX(@nguid);

call find_nhop(x'A6', x'A2');  # answer is HEX(GUID) x'A5'
select HEX(@nguid);

call find_nhop(x'A1', x'A8');  # answer is the Empty set

call delete_nhop(x'A2', x'A5', x'F1');

call find_nhop(x'A2', x'A6');  # answer is HEX(GUID) x'A3'
select HEX(@nguid);

select * from nhops;
call flush_nhops(x'F1');
select * from nhops;
call find_nhop(x'A8', x'B1');  # answer is HEX(GUID) x'A9'
select HEX(@nguid);

delete from guids;
delete from nhops;

# ==============================================================================
