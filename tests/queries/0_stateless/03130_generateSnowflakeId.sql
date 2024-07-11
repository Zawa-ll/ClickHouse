-- Test SQL function 'generateSnowflakeID'

SELECT bitAnd(bitShiftRight(toUInt64(generateSnowflakeID()), 63), 1) = 0; -- check first bit is zero

SELECT generateSnowflakeID(1) = generateSnowflakeID(2); -- disabled common subexpression elimination --> lhs != rhs
SELECT generateSnowflakeID() = generateSnowflakeID(1); -- same as ^^
SELECT generateSnowflakeID(1) = generateSnowflakeID(1); -- enabled common subexpression elimination

SELECT generateSnowflakeID(1, 2); -- { serverError NUMBER_OF_ARGUMENTS_DOESNT_MATCH }

SELECT generateSnowflakeID(); -- Test generateSnowflakeID with default machine_id
SELECT generateSnowflakeID(123, 'dummy_expr'); -- Test generateSnowflakeID with explicit machine_id

SELECT count(*)
FROM
(
    SELECT DISTINCT generateSnowflakeID()
    FROM numbers(100)
);
