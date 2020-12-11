###
# Assess that the user can use a granted privilege without having to logout.
# Assess that a user can no longer use the privilege as soon as it was revoked.
###

from MonetDBtesting.sqltest import SQLTestCase

with SQLTestCase() as tc1:
    tc1.connect(username="monetdb", password="monetdb")
    tc1.execute('CREATE SCHEMA new_schema_as_well').assertSucceeded()
    tc1.execute('SET SCHEMA new_schema_as_well').assertSucceeded()
    tc1.execute('CREATE TABLE test (x int, y int)').assertSucceeded()
    tc1.execute('INSERT INTO test VALUES (-1, -1)').assertSucceeded()

     # Create a new user and grant the right to select.
    tc1.execute('CREATE USER new_user WITH PASSWORD \'new_quite_long_password\' NAME \'newUser\' SCHEMA new_schema_as_well').assertSucceeded()
    tc1.execute('GRANT SELECT ON new_schema_as_well.test TO new_user').assertSucceeded()
    tc1.execute('GRANT UPDATE ON new_schema_as_well.test TO new_user').assertSucceeded()
    tc1.execute('GRANT INSERT ON new_schema_as_well.test TO new_user').assertSucceeded()
    tc1.execute('GRANT DELETE ON new_schema_as_well.test TO new_user').assertSucceeded()

    with SQLTestCase() as tc2:
        # Login the new user, and select.
        tc2.connect(username='new_user', password='new_quite_long_password')
        tc2.execute('SELECT * FROM test').assertSucceeded()
        tc2.execute('UPDATE test SET x = -3 WHERE y = -1').assertSucceeded()
        tc2.execute('INSERT INTO test VALUES (0, 0)').assertSucceeded()
        tc2.execute('DELETE FROM test WHERE y = -2').assertSucceeded()

        # Revoke the right to select from the new user.
        tc1.execute('REVOKE SELECT ON new_schema_as_well.test FROM new_user').assertSucceeded()

        # The new user should not be able to select anymore.
        tc2.execute('UPDATE test SET x = -66 WHERE y = 66').assertFailed()
        tc2.execute('INSERT INTO test VALUES (66, 66)').assertSucceeded()
        tc2.execute('DELETE FROM test WHERE y = -66').assertFailed()
        tc2.execute('SELECT * FROM test').assertFailed()

# import sys, time, pymonetdb, os

# def connect(username, password):
#     return pymonetdb.connect(database = os.getenv('TSTDB'),
#                              hostname = 'localhost',
#                              port = int(os.getenv('MAPIPORT')),
#                              username = username,
#                              password = password,
#                              autocommit = True)

# def query(conn, sql):
#     print(sql)
#     cur = conn.cursor()
#     try:
#         cur.execute(sql)
#     except pymonetdb.OperationalError as e:
#         print("!", e)
#         return
#     r = cur.fetchall()
#     cur.close()
#     print(r)

# def run(conn, sql):
#     print(sql)
#     try:
#         r = conn.execute(sql)
#     except pymonetdb.OperationalError as e:
#         print("!", e)
#         return
#     print('# OK')


# c1 = connect('monetdb', 'monetdb')
# # Create a new schema with a new table.
# run(c1, 'CREATE SCHEMA new_schema_as_well')
# run(c1, 'SET SCHEMA new_schema_as_well')
# run(c1, 'CREATE TABLE test (x int, y int)')
# run(c1, 'INSERT INTO test VALUES (-1, -1)')

# # Create a new user and grant the right to select.
# run(c1, 'CREATE USER new_user WITH PASSWORD \'new_quite_long_password\' NAME \'newUser\' SCHEMA new_schema_as_well')
# run(c1, 'GRANT SELECT ON new_schema_as_well.test TO new_user')
# run(c1, 'GRANT UPDATE ON new_schema_as_well.test TO new_user')
# run(c1, 'GRANT INSERT ON new_schema_as_well.test TO new_user')
# run(c1, 'GRANT DELETE ON new_schema_as_well.test TO new_user')

# # Login the new user, and select.
# c2 = connect('new_user', 'new_quite_long_password')
# query(c2, 'SELECT * FROM test')
# run(c2, 'UPDATE test SET x = -3 WHERE y = -1')
# run(c2, 'INSERT INTO test VALUES (0, 0)')
# run(c2, 'DELETE FROM test WHERE y = -2')

# # Revoke the right to select from the new user.
# run(c1, 'REVOKE SELECT ON new_schema_as_well.test FROM new_user')

# # The new user should not be able to select anymore.
# run(c2, 'UPDATE test SET x = -66 WHERE y = 66')
# run(c2, 'INSERT INTO test VALUES (66, 66)')
# run(c2, 'DELETE FROM test WHERE y = -66')
# query(c2, 'SELECT * FROM test')

