-- start_ignore
create language plpythonu;
-- end_ignore


-- Experimental feature: show memory account usage in EXPLAIN ANALYZE.
SET explain_memory_verbosity=detail;
SET statement_mem=5000;
-- Prints out the memory limit for operators (in explain) assigned by resource queue
SET gp_resqueue_print_operator_memory_limits=ON;
SET gp_workfile_type_hashjoin=bfz;
SET gp_workfile_compress_algorithm=zlib;


-- Given a query, an operator name, and a size, it returns true if the peak of memory used by the operator is greater than the given size; false otherwise. 
-- Inputs: 
--	+ query: an sql statement, where EXPLAIN ANALYZE is used and guc explain_memory_verbosity = detail has been previously set.
--      + operator_name: the name of the operator as appeared in memory account output, e.g., HashJoin => X_HashJoin.
--      + memory_size: size in KB 
create or replace function isMemoryUsedGreaterThan(query text, operator_name text, memory_size int) returns text as
$$
rv = plpy.execute(query)
search_text = operator_name+': Peak'
result = ['false']
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():	
	_start = cur_line.find(search_text) + len(search_text)
	_end = cur_line.find('K bytes', _start)
	if int(cur_line[_start:_end]) > memory_size:
		result = ['true']
        break
return result
$$
language plpythonu;


DROP TABLE IF EXISTS test_hj_spill;
CREATE TABLE test_hj_spill (i1 int, i2 int, i3 int, i4 int, i5 int, i6 int, i7 int, i8 int);

-- Insert data to table so that it will spill workfiles
INSERT INTO test_hj_spill SELECT i,i,i,i,i,i,i,i FROM
	(SELECT generate_series(0, nsegments * 1699999) AS i FROM	
	(SELECT count(*) AS nsegments FROM gp_segment_configuration WHERE role='p' and content >= 0)foo ) bar;

-- The following statement examines if Hash operator uses more than 10MB.
-- If Bfz uses zlib with malloc(), then the allocated memory that we monitor is much less than 10MB.
-- On the other hand, the monitored used memory with Bfz that uses zlib and palloc is much more than 10MB.
-- This test may fail if: i) we modify Hash Join algorithm and spill way less workfiles, or ii) we utilize a version of zlib that allocates much less 
-- memory per open file than version 1.2.3 and 1.2.5 use, i.e., 300KB per open file.
SELECT isMemoryUsedGreaterThan('EXPLAIN ANALYZE SELECT t1.* FROM test_hj_spill AS t1, test_hj_spill AS t2 WHERE t1.i1=t2.i2;', 'X_Hash', 10000);


