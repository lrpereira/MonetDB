SELECT CASE WHEN TRUE THEN -10 ELSE 0/0 END;
SELECT coalesce (CASE WHEN TRUE THEN -10. end, case when false then 0/0 END);
