drop table if exists t1,t2;
drop database if exists mysqltest;
create table t1 (
col1 int not null auto_increment primary key,
col2 varchar(30) not null,
col3 varchar (20) not null,
col4 varchar(4) not null,
col5 enum('PENDING', 'ACTIVE', 'DISABLED') not null,
col6 int not null, to_be_deleted int);
insert into t1 values (2,4,3,5,"PENDING",1,7);
alter table t1
add column col4_5 varchar(20) not null after col4,
add column col7 varchar(30) not null after col5,
add column col8 datetime not null, drop column to_be_deleted,
change column col2 fourth varchar(30) not null after col3,
modify column col6 int not null first;
select * from t1;
col6	col1	col3	fourth	col4	col4_5	col5	col7	col8
1	2	3	4	5		PENDING		0000-00-00 00:00:00
drop table t1;
create table t1 (bandID MEDIUMINT UNSIGNED NOT NULL PRIMARY KEY, payoutID SMALLINT UNSIGNED NOT NULL);
insert into t1 (bandID,payoutID) VALUES (1,6),(2,6),(3,4),(4,9),(5,10),(6,1),(7,12),(8,12);
alter table t1 add column new_col int, order by payoutid,bandid;
select * from t1;
bandID	payoutID	new_col
6	1	NULL
3	4	NULL
1	6	NULL
2	6	NULL
4	9	NULL
5	10	NULL
7	12	NULL
8	12	NULL
alter table t1 order by bandid,payoutid;
select * from t1;
bandID	payoutID	new_col
1	6	NULL
2	6	NULL
3	4	NULL
4	9	NULL
5	10	NULL
6	1	NULL
7	12	NULL
8	12	NULL
drop table t1;
CREATE TABLE t1 (
GROUP_ID int(10) unsigned DEFAULT '0' NOT NULL,
LANG_ID smallint(5) unsigned DEFAULT '0' NOT NULL,
NAME varchar(80) DEFAULT '' NOT NULL,
PRIMARY KEY (GROUP_ID,LANG_ID),
KEY NAME (NAME));
ALTER TABLE t1 CHANGE NAME NAME CHAR(80) not null;
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
GROUP_ID	int(10) unsigned	NULL		PRI	0			
LANG_ID	smallint(5) unsigned	NULL		PRI	0			
NAME	char(80)	latin1_swedish_ci		MUL				
DROP TABLE t1;
create table t1 (n int);
insert into t1 values(9),(3),(12),(10);
alter table t1 order by n;
select * from t1;
n
3
9
10
12
drop table t1;
CREATE TABLE t1 (
id int(11) unsigned NOT NULL default '0',
category_id tinyint(4) unsigned NOT NULL default '0',
type_id tinyint(4) unsigned NOT NULL default '0',
body text NOT NULL,
user_id int(11) unsigned NOT NULL default '0',
status enum('new','old') NOT NULL default 'new',
PRIMARY KEY (id)
) ENGINE=MyISAM;
ALTER TABLE t1 ORDER BY t1.id, t1.status, t1.type_id, t1.user_id, t1.body;
DROP TABLE t1;
CREATE TABLE t1 (AnamneseId int(10) unsigned NOT NULL auto_increment,B BLOB,PRIMARY KEY (AnamneseId)) engine=myisam;
insert into t1 values (null,"hello");
LOCK TABLES t1 WRITE;
ALTER TABLE t1 ADD Column new_col int not null;
UNLOCK TABLES;
OPTIMIZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	OK
DROP TABLE t1;
create table t1 (i int unsigned not null auto_increment primary key);
insert into t1 values (null),(null),(null),(null);
alter table t1 drop i,add i int unsigned not null auto_increment, drop primary key, add primary key (i);
select * from t1;
i
1
2
3
4
drop table t1;
create table t1 (name char(15));
insert into t1 (name) values ("current");
create database mysqltest;
create table mysqltest.t1 (name char(15));
insert into mysqltest.t1 (name) values ("mysqltest");
select * from t1;
name
current
select * from mysqltest.t1;
name
mysqltest
alter table t1 rename mysqltest.t1;
ERROR 42S01: Table 't1' already exists
select * from t1;
name
current
select * from mysqltest.t1;
name
mysqltest
drop table t1;
drop database mysqltest;
create database mysqltest;
create table mysqltest.t1 (a int,b int,c int);
grant all on mysqltest.t1 to mysqltest_1@localhost;
alter table t1 rename t2;
revoke all privileges on mysqltest.t1 from mysqltest_1@localhost;
delete from mysql.user where user=_binary'mysqltest_1';
drop database mysqltest;
create table t1 (n1 int not null, n2 int, n3 int, n4 float,
unique(n1),
key (n1, n2, n3, n4),
key (n2, n3, n4, n1),
key (n3, n4, n1, n2),
key (n4, n1, n2, n3) );
alter table t1 disable keys;
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	n1	1	n1	A	0	NULL	NULL		BTREE	
t1	1	n1_2	1	n1	A	NULL	NULL	NULL		BTREE	disabled
t1	1	n1_2	2	n2	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n1_2	3	n3	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n1_2	4	n4	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n2	1	n2	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n2	2	n3	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n2	3	n4	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n2	4	n1	A	NULL	NULL	NULL		BTREE	disabled
t1	1	n3	1	n3	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n3	2	n4	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n3	3	n1	A	NULL	NULL	NULL		BTREE	disabled
t1	1	n3	4	n2	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n4	1	n4	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n4	2	n1	A	NULL	NULL	NULL		BTREE	disabled
t1	1	n4	3	n2	A	NULL	NULL	NULL	YES	BTREE	disabled
t1	1	n4	4	n3	A	NULL	NULL	NULL	YES	BTREE	disabled
insert into t1 values(10,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(9,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(8,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(7,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(6,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(5,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(4,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(3,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(2,RAND()*1000,RAND()*1000,RAND());
insert into t1 values(1,RAND()*1000,RAND()*1000,RAND());
alter table t1 enable keys;
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	n1	1	n1	A	10	NULL	NULL		BTREE	
t1	1	n1_2	1	n1	A	10	NULL	NULL		BTREE	
t1	1	n1_2	2	n2	A	10	NULL	NULL	YES	BTREE	
t1	1	n1_2	3	n3	A	10	NULL	NULL	YES	BTREE	
t1	1	n1_2	4	n4	A	10	NULL	NULL	YES	BTREE	
t1	1	n2	1	n2	A	10	NULL	NULL	YES	BTREE	
t1	1	n2	2	n3	A	10	NULL	NULL	YES	BTREE	
t1	1	n2	3	n4	A	10	NULL	NULL	YES	BTREE	
t1	1	n2	4	n1	A	10	NULL	NULL		BTREE	
t1	1	n3	1	n3	A	10	NULL	NULL	YES	BTREE	
t1	1	n3	2	n4	A	10	NULL	NULL	YES	BTREE	
t1	1	n3	3	n1	A	10	NULL	NULL		BTREE	
t1	1	n3	4	n2	A	10	NULL	NULL	YES	BTREE	
t1	1	n4	1	n4	A	10	NULL	NULL	YES	BTREE	
t1	1	n4	2	n1	A	10	NULL	NULL		BTREE	
t1	1	n4	3	n2	A	10	NULL	NULL	YES	BTREE	
t1	1	n4	4	n3	A	10	NULL	NULL	YES	BTREE	
drop table t1;
create table t1 (i int unsigned not null auto_increment primary key);
alter table t1 rename t2;
alter table t2 rename t1, add c char(10) comment "no comment";
show columns from t1;
Field	Type	Null	Key	Default	Extra
i	int(10) unsigned		PRI	NULL	auto_increment
c	char(10)	YES		NULL	
drop table t1;
create table t1 (a int, b int);
insert into t1 values(1,100), (2,100), (3, 100);
insert into t1 values(1,99), (2,99), (3, 99);
insert into t1 values(1,98), (2,98), (3, 98);
insert into t1 values(1,97), (2,97), (3, 97);
insert into t1 values(1,96), (2,96), (3, 96);
insert into t1 values(1,95), (2,95), (3, 95);
insert into t1 values(1,94), (2,94), (3, 94);
insert into t1 values(1,93), (2,93), (3, 93);
insert into t1 values(1,92), (2,92), (3, 92);
insert into t1 values(1,91), (2,91), (3, 91);
insert into t1 values(1,90), (2,90), (3, 90);
insert into t1 values(1,89), (2,89), (3, 89);
insert into t1 values(1,88), (2,88), (3, 88);
insert into t1 values(1,87), (2,87), (3, 87);
insert into t1 values(1,86), (2,86), (3, 86);
insert into t1 values(1,85), (2,85), (3, 85);
insert into t1 values(1,84), (2,84), (3, 84);
insert into t1 values(1,83), (2,83), (3, 83);
insert into t1 values(1,82), (2,82), (3, 82);
insert into t1 values(1,81), (2,81), (3, 81);
insert into t1 values(1,80), (2,80), (3, 80);
insert into t1 values(1,79), (2,79), (3, 79);
insert into t1 values(1,78), (2,78), (3, 78);
insert into t1 values(1,77), (2,77), (3, 77);
insert into t1 values(1,76), (2,76), (3, 76);
insert into t1 values(1,75), (2,75), (3, 75);
insert into t1 values(1,74), (2,74), (3, 74);
insert into t1 values(1,73), (2,73), (3, 73);
insert into t1 values(1,72), (2,72), (3, 72);
insert into t1 values(1,71), (2,71), (3, 71);
insert into t1 values(1,70), (2,70), (3, 70);
insert into t1 values(1,69), (2,69), (3, 69);
insert into t1 values(1,68), (2,68), (3, 68);
insert into t1 values(1,67), (2,67), (3, 67);
insert into t1 values(1,66), (2,66), (3, 66);
insert into t1 values(1,65), (2,65), (3, 65);
insert into t1 values(1,64), (2,64), (3, 64);
insert into t1 values(1,63), (2,63), (3, 63);
insert into t1 values(1,62), (2,62), (3, 62);
insert into t1 values(1,61), (2,61), (3, 61);
insert into t1 values(1,60), (2,60), (3, 60);
insert into t1 values(1,59), (2,59), (3, 59);
insert into t1 values(1,58), (2,58), (3, 58);
insert into t1 values(1,57), (2,57), (3, 57);
insert into t1 values(1,56), (2,56), (3, 56);
insert into t1 values(1,55), (2,55), (3, 55);
insert into t1 values(1,54), (2,54), (3, 54);
insert into t1 values(1,53), (2,53), (3, 53);
insert into t1 values(1,52), (2,52), (3, 52);
insert into t1 values(1,51), (2,51), (3, 51);
insert into t1 values(1,50), (2,50), (3, 50);
insert into t1 values(1,49), (2,49), (3, 49);
insert into t1 values(1,48), (2,48), (3, 48);
insert into t1 values(1,47), (2,47), (3, 47);
insert into t1 values(1,46), (2,46), (3, 46);
insert into t1 values(1,45), (2,45), (3, 45);
insert into t1 values(1,44), (2,44), (3, 44);
insert into t1 values(1,43), (2,43), (3, 43);
insert into t1 values(1,42), (2,42), (3, 42);
insert into t1 values(1,41), (2,41), (3, 41);
insert into t1 values(1,40), (2,40), (3, 40);
insert into t1 values(1,39), (2,39), (3, 39);
insert into t1 values(1,38), (2,38), (3, 38);
insert into t1 values(1,37), (2,37), (3, 37);
insert into t1 values(1,36), (2,36), (3, 36);
insert into t1 values(1,35), (2,35), (3, 35);
insert into t1 values(1,34), (2,34), (3, 34);
insert into t1 values(1,33), (2,33), (3, 33);
insert into t1 values(1,32), (2,32), (3, 32);
insert into t1 values(1,31), (2,31), (3, 31);
insert into t1 values(1,30), (2,30), (3, 30);
insert into t1 values(1,29), (2,29), (3, 29);
insert into t1 values(1,28), (2,28), (3, 28);
insert into t1 values(1,27), (2,27), (3, 27);
insert into t1 values(1,26), (2,26), (3, 26);
insert into t1 values(1,25), (2,25), (3, 25);
insert into t1 values(1,24), (2,24), (3, 24);
insert into t1 values(1,23), (2,23), (3, 23);
insert into t1 values(1,22), (2,22), (3, 22);
insert into t1 values(1,21), (2,21), (3, 21);
insert into t1 values(1,20), (2,20), (3, 20);
insert into t1 values(1,19), (2,19), (3, 19);
insert into t1 values(1,18), (2,18), (3, 18);
insert into t1 values(1,17), (2,17), (3, 17);
insert into t1 values(1,16), (2,16), (3, 16);
insert into t1 values(1,15), (2,15), (3, 15);
insert into t1 values(1,14), (2,14), (3, 14);
insert into t1 values(1,13), (2,13), (3, 13);
insert into t1 values(1,12), (2,12), (3, 12);
insert into t1 values(1,11), (2,11), (3, 11);
insert into t1 values(1,10), (2,10), (3, 10);
insert into t1 values(1,9), (2,9), (3, 9);
insert into t1 values(1,8), (2,8), (3, 8);
insert into t1 values(1,7), (2,7), (3, 7);
insert into t1 values(1,6), (2,6), (3, 6);
insert into t1 values(1,5), (2,5), (3, 5);
insert into t1 values(1,4), (2,4), (3, 4);
insert into t1 values(1,3), (2,3), (3, 3);
insert into t1 values(1,2), (2,2), (3, 2);
insert into t1 values(1,1), (2,1), (3, 1);
alter table t1 add unique (a,b), add key (b);
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	a	1	a	A	NULL	NULL	NULL	YES	BTREE	
t1	0	a	2	b	A	NULL	NULL	NULL	YES	BTREE	
t1	1	b	1	b	A	100	NULL	NULL	YES	BTREE	
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	a	1	a	A	3	NULL	NULL	YES	BTREE	
t1	0	a	2	b	A	300	NULL	NULL	YES	BTREE	
t1	1	b	1	b	A	100	NULL	NULL	YES	BTREE	
drop table t1;
CREATE TABLE t1 (i int(10), index(i) );
ALTER TABLE t1 DISABLE KEYS;
INSERT DELAYED INTO t1 VALUES(1),(2),(3);
ALTER TABLE t1 ENABLE KEYS;
drop table t1;
set names koi8r;
create table t1 (a char(10) character set koi8r);
insert into t1 values ('����');
select a,hex(a) from t1;
a	hex(a)
����	D4C5D3D4
alter table t1 change a a char(10) character set cp1251;
select a,hex(a) from t1;
a	hex(a)
����	F2E5F1F2
alter table t1 change a a binary(10);
select a,hex(a) from t1;
a	hex(a)
����	F2E5F1F2
alter table t1 change a a char(10) character set cp1251;
select a,hex(a) from t1;
a	hex(a)
����	F2E5F1F2
alter table t1 change a a char(10) character set koi8r;
select a,hex(a) from t1;
a	hex(a)
����	D4C5D3D4
alter table t1 change a a varchar(10) character set cp1251;
select a,hex(a) from t1;
a	hex(a)
����	F2E5F1F2
alter table t1 change a a char(10) character set koi8r;
select a,hex(a) from t1;
a	hex(a)
����	D4C5D3D4
alter table t1 change a a text character set cp1251;
select a,hex(a) from t1;
a	hex(a)
����	F2E5F1F2
alter table t1 change a a char(10) character set koi8r;
select a,hex(a) from t1;
a	hex(a)
����	D4C5D3D4
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(10) character set koi8r default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
alter table t1 DEFAULT CHARACTER SET latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(10) character set koi8r default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
alter table t1 CONVERT TO CHARACTER SET latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(10) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
alter table t1 DEFAULT CHARACTER SET cp1251;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(10) character set latin1 default NULL
) ENGINE=MyISAM DEFAULT CHARSET=cp1251
drop table t1;
create table t1 (myblob longblob,mytext longtext) 
default charset latin1 collate latin1_general_cs;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `myblob` longblob,
  `mytext` longtext collate latin1_general_cs
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_general_cs
alter table t1 character set latin2;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `myblob` longblob,
  `mytext` longtext character set latin1 collate latin1_general_cs
) ENGINE=MyISAM DEFAULT CHARSET=latin2
drop table t1;
CREATE TABLE t1 (
Host varchar(16) binary NOT NULL default '',
User varchar(16) binary NOT NULL default '',
PRIMARY KEY  (Host,User)
) ENGINE=MyISAM;
ALTER TABLE t1 DISABLE KEYS;
LOCK TABLES t1 WRITE;
INSERT INTO t1 VALUES ('localhost','root'),('localhost',''),('games','monty');
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	Host	A	NULL	NULL	NULL		BTREE	
t1	0	PRIMARY	2	User	A	3	NULL	NULL		BTREE	
ALTER TABLE t1 ENABLE KEYS;
UNLOCK TABLES;
CHECK TABLES t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
CREATE TABLE t1 (
Host varchar(16) binary NOT NULL default '',
User varchar(16) binary NOT NULL default '',
PRIMARY KEY  (Host,User),
KEY  (Host)
) ENGINE=MyISAM;
ALTER TABLE t1 DISABLE KEYS;
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	Host	A	NULL	NULL	NULL		BTREE	
t1	0	PRIMARY	2	User	A	0	NULL	NULL		BTREE	
t1	1	Host	1	Host	A	NULL	NULL	NULL		BTREE	disabled
LOCK TABLES t1 WRITE;
INSERT INTO t1 VALUES ('localhost','root'),('localhost','');
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	Host	A	NULL	NULL	NULL		BTREE	
t1	0	PRIMARY	2	User	A	2	NULL	NULL		BTREE	
t1	1	Host	1	Host	A	NULL	NULL	NULL		BTREE	disabled
ALTER TABLE t1 ENABLE KEYS;
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	Host	A	NULL	NULL	NULL		BTREE	
t1	0	PRIMARY	2	User	A	2	NULL	NULL		BTREE	
t1	1	Host	1	Host	A	1	NULL	NULL		BTREE	
UNLOCK TABLES;
CHECK TABLES t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
LOCK TABLES t1 WRITE;
ALTER TABLE t1 RENAME t2;
UNLOCK TABLES;
select * from t2;
Host	User
localhost	
localhost	root
DROP TABLE t2;
CREATE TABLE t1 (
Host varchar(16) binary NOT NULL default '',
User varchar(16) binary NOT NULL default '',
PRIMARY KEY  (Host,User),
KEY  (Host)
) ENGINE=MyISAM;
LOCK TABLES t1 WRITE;
ALTER TABLE t1 DISABLE KEYS;
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	Host	A	NULL	NULL	NULL		BTREE	
t1	0	PRIMARY	2	User	A	0	NULL	NULL		BTREE	
t1	1	Host	1	Host	A	NULL	NULL	NULL		BTREE	disabled
DROP TABLE t1;
CREATE TABLE t1 (a int PRIMARY KEY, b INT UNIQUE);
ALTER TABLE t1 DROP PRIMARY KEY;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int(11) NOT NULL default '0',
  `b` int(11) default NULL,
  UNIQUE KEY `b` (`b`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1
ALTER TABLE t1 DROP PRIMARY KEY;
ERROR 42000: Can't DROP 'PRIMARY'; check that column/key exists
DROP TABLE t1;
create table t1 (a int, b int, key(a));
insert into t1 values (1,1), (2,2);
alter table t1 drop key no_such_key;
ERROR 42000: Can't DROP 'no_such_key'; check that column/key exists
alter table t1 drop key a;
drop table t1;
create table t1 (a int);
alter table t1 rename to `t1\\`;
ERROR 42000: Incorrect table name 't1\\'
rename table t1 to `t1\\`;
ERROR 42000: Incorrect table name 't1\\'
drop table t1;
