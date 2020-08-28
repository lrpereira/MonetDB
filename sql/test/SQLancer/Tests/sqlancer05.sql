CREATE TABLE "sys"."t1" (
	"c0" INTERVAL SECOND NOT NULL,
	CONSTRAINT "t1_c0_pkey" PRIMARY KEY ("c0"),
	CONSTRAINT "t1_c0_unique" UNIQUE ("c0")
);
INSERT INTO t1(c0) VALUES(INTERVAL '1474617942' SECOND), (NULL);
INSERT INTO t1(c0) VALUES(INTERVAL '1427502482' SECOND), (INTERVAL '-1598854979' SECOND); 
DELETE FROM t1 WHERE CASE r'FALSE' WHEN r'879628043' THEN TRUE ELSE ((((t1.c0)>=(t1.c0)))OR(((TRUE)OR(TRUE)))) END;
INSERT INTO t1(c0) VALUES(INTERVAL '236620278' SECOND);
INSERT INTO t1(c0) VALUES(INTERVAL '1448897349' SECOND);
DELETE FROM t1 WHERE CAST(TRUE AS BOOLEAN);
	-- all rows deleted
DROP TABLE sys.t1;

START TRANSACTION;
CREATE TABLE "sys"."t0" ("c0" TIMESTAMP,"c1" INTERVAL MONTH,CONSTRAINT "t0_c1_unique" UNIQUE ("c1"));
CREATE TABLE "sys"."t1" ("c0" BOOLEAN, "c1" DECIMAL(18,3) NOT NULL);
COPY 9 RECORDS INTO "sys"."t1" FROM stdin USING DELIMITERS E'\t',E'\n','"';
NULL	0.526
true	0.259
true	0.382
NULL	0.994
NULL	0.101
NULL	0.713
NULL	0.433
NULL	0.899
NULL	0.239

CREATE TABLE "sys"."t2" ("c0" BOOLEAN, "c1" DECIMAL(18,3));
COPY 3 RECORDS INTO "sys"."t2" FROM stdin USING DELIMITERS E'\t',E'\n','"';
NULL	0.692
NULL	0.860
NULL	0.230

SELECT max(t2.c1) FROM t1 LEFT OUTER JOIN t2 ON CASE WHEN t2.c0 < t1.c0 OR t1.c0 THEN t1.c0 WHEN t1.c0 THEN t1.c0 END;
	-- 0.860
create view v0(c0) as (select all 7.948668E7 from t1) with check option;
SELECT max(t2.c1) FROM t0, t1 CROSS JOIN v0 LEFT OUTER JOIN t2 
ON CASE WHEN ((((((((v0.c0)=(v0.c0)))OR(((t1.c0)>=(t1.c0)))))AND(((t2.c0)<(t1.c0)))))OR(((((t1.c0)AND(t1.c0)))AND(t1.c0)))) 
THEN t1.c0 WHEN COALESCE(COALESCE(TRUE, t2.c0, t1.c0), CASE t1.c1 WHEN t2.c1 THEN t2.c0 WHEN t1.c1 THEN t2.c0 ELSE t1.c0 END) 
THEN t2.c0 WHEN t1.c0 THEN t1.c0 END;
rollback;

START TRANSACTION;
CREATE TABLE t0("c1" INTEGER NOT NULL,CONSTRAINT "t0_c1_pkey" PRIMARY KEY ("c1"),CONSTRAINT "t0_c1_unique" UNIQUE ("c1"));
COPY 8 RECORDS INTO t0 FROM stdin USING DELIMITERS E'\t',E'\n','"';
262015489
-1667887296
1410307212
1073218199
-167204307
1394786866
1112194034
2140251980

SELECT 1 FROM (select time '12:43:09' from t0) as v0(c0) RIGHT OUTER JOIN (SELECT INTERVAL '2' SECOND FROM t0) AS sub0 ON 
TIME '07:04:19' BETWEEN CASE 'b' WHEN 'a' THEN v0.c0 ELSE v0.c0 END AND v0.c0;
	--8 rows of 1
create view v0(c0, c1) as (select all time '12:43:09', interval '1251003346' second from t0) with check option;
SELECT count(ALL - (CAST(NULL AS INT))) FROM v0 RIGHT OUTER JOIN (SELECT INTERVAL '1380374779' SECOND FROM t0) AS sub0 ON 
COALESCE(TRUE, (TIME '07:04:19') BETWEEN SYMMETRIC (CASE r'漈' WHEN r'T㊆ßwU.H' THEN v0.c0 ELSE v0.c0 END) AND (v0.c0));
rollback;

START TRANSACTION;
CREATE TABLE "sys"."t2" (
	"c0" BOOLEAN,
	"c1" DOUBLE NOT NULL,
	CONSTRAINT "t2_c1_pkey" PRIMARY KEY ("c1"),
	CONSTRAINT "t2_c0_c1_unique" UNIQUE ("c0", "c1")
);
COPY 3 RECORDS INTO "sys"."t2" FROM stdin USING DELIMITERS E'\t',E'\n','"';
NULL	0.385788843525793
true	0.6696036757274413
false	0.3589261452091549

INSERT INTO t2 VALUES (CASE WHEN TRUE THEN TIMESTAMP '1970-01-11 13:04:02' END BETWEEN TIMESTAMP '1970-01-05 05:04:47' AND TIMESTAMP '1970-01-01 20:00:35', 1);
INSERT INTO t2 VALUES(NOT ((CASE WHEN TRUE THEN TIMESTAMP '1970-01-11 13:04:02' ELSE TIMESTAMP '1970-01-02 23:25:05' END) 
NOT BETWEEN SYMMETRIC (COALESCE(TIMESTAMP '1969-12-30 12:07:22', TIMESTAMP '1970-01-05 05:04:47')) AND (CASE WHEN TRUE THEN TIMESTAMP '1970-01-01 20:00:35' END)), 2);
ROLLBACK;

START TRANSACTION;
CREATE TABLE "sys"."t1" ("c0" TIMESTAMP NOT NULL,CONSTRAINT "t1_c0_pkey" PRIMARY KEY ("c0"));
CREATE TABLE "sys"."t2" ("c0" TIMESTAMP);
COPY 3 RECORDS INTO "sys"."t2" FROM stdin USING DELIMITERS E'\t',E'\n','"';
"1970-01-25 08:23:04.000000"
"1970-01-06 08:19:06.000000"
"1970-01-19 15:18:44.000000"

create view v0(c0) as (select all -1454749390 from t2, t1 where not 
((interval '990585801' month) in (interval '1558064353' month, interval '1877885111' month, interval '1286819945' month))) 
with check option;

SELECT CAST(SUM(agg0) AS BIGINT) FROM (
SELECT count(ALL + (((v0.c0)/(((v0.c0)^(v0.c0)))))) as agg0 FROM v0 WHERE CAST(CAST(v0.c0 AS STRING) AS BOOLEAN)
UNION ALL
SELECT count(ALL + (((v0.c0)/(((v0.c0)^(v0.c0)))))) as agg0 FROM v0 WHERE NOT (CAST(CAST(v0.c0 AS STRING) AS BOOLEAN))
UNION ALL
SELECT count(ALL + (((v0.c0)/(((v0.c0)^(v0.c0)))))) as agg0 FROM v0 WHERE (CAST(CAST(v0.c0 AS STRING) AS BOOLEAN)) IS NULL)
as asdf;
	-- 0
SELECT count(ALL + (((v0.c0)/(((v0.c0)^(v0.c0)))))) FROM v0;
	-- 0
SELECT count(ALL + (((v0.c0)/(((v0.c0)^(v0.c0)))))) FROM v0 ORDER BY v0.c0 ASC NULLS LAST, v0.c0 DESC NULLS FIRST, v0.c0 DESC NULLS LAST;
	--error, v0.c0 cannot be used without an aggregate function
ROLLBACK;

-- This crash only happens on auto-commit mode
CREATE TABLE "sys"."t1" ("c0" DECIMAL(18,3) NOT NULL,CONSTRAINT "t1_c0_pkey" PRIMARY KEY ("c0"));
COPY 2 RECORDS INTO "sys"."t1" FROM stdin USING DELIMITERS E'\t',E'\n','"';
0.403
0.008

DELETE FROM t1 WHERE (((0.86983466) IS NOT NULL) = TRUE) = TRUE;
INSERT INTO t1(c0) VALUES(0.40), (0.75);
UPDATE t1 SET c0 = 0.28 WHERE (CAST(INTERVAL '31' MONTH AS STRING)) NOT IN (COALESCE('靟', 'P먌+}I*CpQ'));
SELECT c0 FROM t1;
DROP TABLE t1;

SELECT 1 WHERE '0';
	--empty
SELECT 1 WHERE NOT '0';
	-- 1
SELECT 1 WHERE '0' IS NULL;
	--empty

CREATE TABLE "sys"."t0" ("c2" DATE NOT NULL DEFAULT DATE '1970-01-03',
	CONSTRAINT "t0_c2_pkey" PRIMARY KEY ("c2"),
	CONSTRAINT "t0_c2_unique" UNIQUE ("c2"),
	CONSTRAINT "t0_c2_fkey" FOREIGN KEY ("c2") REFERENCES "sys"."t0" ("c2")
);
CREATE TABLE "sys"."t1" ("c0" CHARACTER LARGE OBJECT NOT NULL,CONSTRAINT "t1_c0_unique" UNIQUE ("c0"));
COPY 5 RECORDS INTO "sys"."t1" FROM stdin USING DELIMITERS E'\t',E'\n','"';
"t8rMম<b\015"
"}鸺s"
""
"!5Ef"
">l\tigL쓵9Q"

CREATE TABLE "sys"."t2" ("c0" TIME NOT NULL, "c1" DECIMAL(18,3) NOT NULL DEFAULT 0.428153, "c2" TIME,
	CONSTRAINT "t2_c2_c0_c1_unique" UNIQUE ("c2", "c0", "c1")
);
COPY 1 RECORDS INTO "sys"."t2" FROM stdin USING DELIMITERS E'\t',E'\n','"';
05:17:55	0.450	02:23:09

create view v0(c0, c1) as (select t2.c2, cast(timestamp '1970-01-01 17:26:21' as timestamp) from t0, t2);
create view v1(c0) as (select distinct 0.73 from t2, t1, t0, v0) with check option;

SELECT 1 FROM v0 NATURAL JOIN (SELECT t1.c0, t2.c0 FROM t2, t1) AS sub0;
	-- error, common column name "c0" appears more than once in right table
SELECT count(ALL + (((((18)%(-16)))/(CAST(TRUE AS INT))))) FROM t1, v1, t0 FULL OUTER JOIN v0 
ON (CAST(+ (-116) AS BOOLEAN)) = FALSE RIGHT OUTER JOIN t2 
ON (COALESCE(v0.c1, v0.c1, v0.c1, v0.c1)) NOT IN (TIMESTAMP '1970-01-15 03:05:56', TIMESTAMP '1970-01-14 22:30:41') 
NATURAL JOIN (SELECT t1.c0, CAST(48 AS INT), t2.c0 FROM t2, v0, v1, t0, t1) AS sub0;
	-- error, common column name "c0" appears more than once in right table
DROP TABLE t0 cascade;
DROP TABLE t1 cascade;
DROP TABLE t2 cascade;

select sub0.c0 from (select 1 as c0, 2 as c0) as sub0;
	--error, column reference "c0" is ambiguous

START TRANSACTION;
CREATE TABLE "sys"."t0" ("c0" VARCHAR(31) NOT NULL,"c1" BOOLEAN,"c2" TIMESTAMP NOT NULL,
	CONSTRAINT "t0_c0_c2_c1_unique" UNIQUE ("c0", "c2", "c1"),
	CONSTRAINT "t0_c2_unique" UNIQUE ("c2"),
	CONSTRAINT "t0_c1_c0_unique" UNIQUE ("c1", "c0"),
	CONSTRAINT "t0_c0_unique" UNIQUE ("c0")
);
CREATE TABLE "sys"."t1" ("c1" INTERVAL MONTH NOT NULL,CONSTRAINT "t1_c1_pkey" PRIMARY KEY ("c1"),CONSTRAINT "t1_c1_unique" UNIQUE ("c1"));
COPY 2 RECORDS INTO "sys"."t1" FROM stdin USING DELIMITERS E'\t',E'\n','"';
311141409
1247645829

CREATE TABLE "sys"."t2" ("c1" BIGINT NOT NULL,CONSTRAINT "t2_c1_pkey" PRIMARY KEY ("c1"),CONSTRAINT "t2_c1_unique" UNIQUE ("c1"));
COPY 4 RECORDS INTO "sys"."t2" FROM stdin USING DELIMITERS E'\t',E'\n','"';
1909667273
720758463
24545308
-1804793562

create view v0(c0, c1) as (select distinct t0.c0, t0.c2 from t2, t0 where t0.c1);

SELECT 1 FROM v0, t0 JOIN t1 ON t0.c1 WHERE (CASE CASE t0.c0 WHEN t0.c0 THEN t1.c1 END WHEN t1.c1 THEN v0.c1 END) BETWEEN (v0.c1) AND (t0.c2);
	-- empty
SELECT count(agg0) FROM (
SELECT ALL sum(ALL t1.c1) as agg0 FROM v0, t0 JOIN t1 ON t0.c1 CROSS JOIN t2 WHERE (CASE CASE t0.c0 WHEN t0.c0 THEN t1.c1 END WHEN t1.c1 THEN v0.c1 END) NOT  BETWEEN ASYMMETRIC (v0.c1) AND (t0.c2)
UNION ALL
SELECT ALL sum(ALL t1.c1) as agg0 FROM v0, t0 JOIN t1 ON t0.c1 CROSS JOIN t2 WHERE NOT ((CASE CASE t0.c0 WHEN t0.c0 THEN t1.c1 END WHEN t1.c1 THEN v0.c1 END) NOT  BETWEEN ASYMMETRIC (v0.c1) AND (t0.c2))
UNION ALL
SELECT ALL sum(ALL t1.c1) as agg0 FROM v0, t0 JOIN t1 ON t0.c1 CROSS JOIN t2 WHERE ((CASE CASE t0.c0 WHEN t0.c0 THEN t1.c1 END WHEN t1.c1 THEN v0.c1 END) NOT  BETWEEN ASYMMETRIC (v0.c1) AND (t0.c2)) IS NULL
) as asdf;
	-- 0
ROLLBACK;

START TRANSACTION;
CREATE TABLE "t1" ("c0" INTERVAL MONTH NOT NULL,CONSTRAINT "t1_c0_pkey" PRIMARY KEY ("c0"));
COPY 5 RECORDS INTO "t1" FROM stdin USING DELIMITERS E'\t',E'\n','"';
141227804
1365727954
498933006
1181578353
651226237

SELECT CAST(SUM(count) AS BIGINT) FROM (SELECT CASE INTERVAL '-1' MONTH WHEN INTERVAL '2' MONTH 
THEN NOT t1.c0 BETWEEN t1.c0 AND t1.c0 ELSE t1.c0 < CASE FALSE WHEN TRUE THEN t1.c0 ELSE t1.c0 END END as count FROM t1) 
as res;
	-- 0
SELECT CAST(SUM(count) AS BIGINT) FROM (SELECT CAST(CASE INTERVAL '-803989404' MONTH WHEN INTERVAL '795326851' MONTH 
THEN NOT ((t1.c0) BETWEEN ASYMMETRIC (t1.c0) AND (t1.c0)) ELSE ((t1.c0)<(CASE FALSE WHEN TRUE THEN t1.c0 ELSE t1.c0 END)) END AS INT) as count FROM t1) 
as res;
	-- 0
ROLLBACK;
