CREATE  STREAM TABLE strt  (id int primary key, nm varchar(123) NOT NULL);
CREATE merge TABLE strt  (id int primary key, nm varchar(123) NOT NULL);

with describe_all_objects as (
 select s.name as sname,
     t.name,
     s.name || '.' || t.name as fullname,
     cast(case t.type
     when 1 then 2
     else 1
     end as smallint) as ntype,
     (case when t.system then 'SYSTEM ' else '' end) || tt.table_type_name as type,
     t.system
   from sys._tables t
   left outer join sys.schemas s on t.schema_id = s.id
   left outer join sys.table_types tt on t.type = tt.table_type_id )
select type, fullname from describe_all_objects where (ntype & 3) > 0 and not system and (sname is null or sname = current_schema) order by fullname, type;

SELECT table_id, * FROM sys._columns WHERE (table_id) NOT IN (SELECT id FROM sys._tables);
-- no rows

DROP TABLE strt;

with describe_all_objects as (
 select s.name as sname,
     t.name,
     s.name || '.' || t.name as fullname,
     cast(case t.type
     when 1 then 2
     else 1
     end as smallint) as ntype,
     (case when t.system then 'SYSTEM ' else '' end) || tt.table_type_name as type,
     t.system
   from sys._tables t
   left outer join sys.schemas s on t.schema_id = s.id
   left outer join sys.table_types tt on t.type = tt.table_type_id )
select type, fullname from describe_all_objects where (ntype & 3) > 0 and not system and (sname is null or sname = current_schema) order by fullname, type;

SELECT table_id, * FROM sys._columns WHERE (table_id) NOT IN (SELECT id FROM sys._tables);
-- shows 2 columns which reference a table_id which does not exist in sys._tables
