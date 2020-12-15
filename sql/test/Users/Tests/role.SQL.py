###
# Tests for roles and users
#
# 1. check that we cannot DROP an unexisting ROLE.
# 2. check that it is not possible to reCREATE an existing ROLE.
# 3. check that we cannot DROP a ROLE GRANTed TO a USER
# 4. check that a USER can SET a GRANTed ROLE but cannot SET a non-GRANTed ROLE
# 5. check that we can DROP a ROLE after REVOKE
# 6. check that we cannot REVOKE a non-GRANTed ROLE
# 7. check that afer a ROLE is REVOKEd the USER can no longer assume it.
###

from MonetDBtesting.sqltest import SQLTestCase

with SQLTestCase() as tc:
    tc.connect(username="monetdb", password="monetdb")

    tc.execute("DROP ROLE non_existing_role;").assertFailed(err_code="0P000", err_message="DROP ROLE: no such role 'non_existing_role'")

    tc.execute("CREATE ROLE role1;").assertSucceeded()
    tc.execute("CREATE ROLE role2;").assertSucceeded()
    tc.execute("CREATE ROLE role3;").assertSucceeded()
    tc.execute("CREATE ROLE role1;").assertFailed(err_code="0P000", err_message="Role 'role1' already exists")

    tc.execute("CREATE USER alice with password 'alice' name 'alice' schema sys;")
    tc.execute("GRANT role1 to alice;").assertSucceeded()
    tc.execute("GRANT role2 to alice;").assertSucceeded()
    #tc.execute("DROP ROLE role1;").assertFailed()

    tc.connect(username="alice", password="alice")
    tc.execute("SET ROLE role1;").assertSucceeded()
    tc.execute("SET ROLE role3;").assertFailed(err_code="42000", err_message="Role (role3) missing")

    tc.connect(username="monetdb", password="monetdb")
    tc.execute("REVOKE role1 from alice;").assertSucceeded()
    tc.execute("REVOKE role2 from alice;").assertSucceeded()
    tc.execute("DROP ROLE role2;").assertSucceeded()

    tc.execute("REVOKE role3 from alice;").assertFailed(err_code="01006", err_message="REVOKE: User 'alice' does not have ROLE 'role3'")

    tc.connect(username="alice", password="alice")
    tc.execute("SET ROLE role1;").assertFailed(err_code="42000", err_message="Role (role1) missing")
    tc.execute("SET ROLE role2;").assertFailed(err_code="42000", err_message="Role (role2) missing")

