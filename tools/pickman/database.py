# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Canonical Ltd.
# Written by Simon Glass <simon.glass@canonical.com>
#
"""Database for pickman - tracks cherry-pick state.

This uses sqlite3 with a local file (.pickman.db).

To adjust the schema, increment LATEST, create a _migrate_to_v<x>() function
and add code in migrate_to() to call it.
"""

from datetime import datetime
import os
import sqlite3

from u_boot_pylib import tools
from u_boot_pylib import tout

# Schema version (version 0 means there is no database yet)
LATEST = 4

# Default database filename
DB_FNAME = '.pickman.db'


class Database:  # pylint: disable=too-many-public-methods
    """Database of cherry-pick state used by pickman"""

    # dict of databases:
    #   key: filename
    #   value: Database object
    instances = {}

    def __init__(self, db_path):
        """Set up a new database object

        Args:
            db_path (str): Path to the database
        """
        if db_path in Database.instances:
            raise ValueError(f"There is already a database for '{db_path}'")
        self.con = None
        self.cur = None
        self.db_path = db_path
        self.is_open = False
        Database.instances[db_path] = self

    @staticmethod
    def get_instance(db_path):
        """Get the database instance for a path

        Args:
            db_path (str): Path to the database

        Return:
            tuple:
                Database: Database instance, created if necessary
                bool: True if newly created
        """
        dbs = Database.instances.get(db_path)
        if dbs:
            return dbs, False
        return Database(db_path), True

    def start(self):
        """Open the database ready for use, migrate to latest schema"""
        self.open_it()
        self.migrate_to(LATEST)

    def open_it(self):
        """Open the database, creating it if necessary"""
        if self.is_open:
            raise ValueError('Already open')
        if not os.path.exists(self.db_path):
            tout.warning(f'Creating new database {self.db_path}')
        self.con = sqlite3.connect(self.db_path)
        self.cur = self.con.cursor()
        self.is_open = True
        Database.instances[self.db_path] = self

    def close(self):
        """Close the database"""
        if not self.is_open:
            raise ValueError('Already closed')
        self.con.close()
        self.cur = None
        self.con = None
        self.is_open = False
        Database.instances.pop(self.db_path, None)

    def _create_v1(self):
        """Create a database with the v1 schema"""
        # Table for tracking source branches and their last cherry-picked commit
        self.cur.execute(
            'CREATE TABLE source ('
            'id INTEGER PRIMARY KEY AUTOINCREMENT, '
            'name TEXT UNIQUE, '
            'last_commit TEXT)')

        # Schema version table
        self.cur.execute('CREATE TABLE schema_version (version INTEGER)')

    def _create_v2(self):
        """Migrate database to v2 schema - add commit and mergereq tables"""
        # Table for tracking individual commits
        self.cur.execute(
            'CREATE TABLE pcommit ('
            'id INTEGER PRIMARY KEY AUTOINCREMENT, '
            'chash TEXT UNIQUE, '
            'source_id INTEGER, '
            'mergereq_id INTEGER, '
            'subject TEXT, '
            'author TEXT, '
            'status TEXT, '
            'cherry_hash TEXT, '
            'FOREIGN KEY (source_id) REFERENCES source(id), '
            'FOREIGN KEY (mergereq_id) REFERENCES mergereq(id))')

        # Table for tracking merge requests
        self.cur.execute(
            'CREATE TABLE mergereq ('
            'id INTEGER PRIMARY KEY AUTOINCREMENT, '
            'source_id INTEGER, '
            'branch_name TEXT, '
            'mr_id INTEGER, '
            'status TEXT, '
            'url TEXT, '
            'created_at TEXT, '
            'FOREIGN KEY (source_id) REFERENCES source(id))')

    def _create_v3(self):
        """Migrate database to v3 schema - add comment table"""
        # Table for tracking processed MR comments
        self.cur.execute(
            'CREATE TABLE comment ('
            'id INTEGER PRIMARY KEY AUTOINCREMENT, '
            'mr_iid INTEGER, '
            'comment_id INTEGER, '
            'processed_at TEXT, '
            'UNIQUE(mr_iid, comment_id))')

    def _create_v4(self):
        """Migrate database to v4 schema - add pipeline_fix table"""
        # Table for tracking pipeline fix attempts per MR
        self.cur.execute(
            'CREATE TABLE pipeline_fix ('
            'id INTEGER PRIMARY KEY AUTOINCREMENT, '
            'mr_iid INTEGER, '
            'pipeline_id INTEGER, '
            'attempt INTEGER, '
            'status TEXT, '
            'created_at TEXT, '
            'UNIQUE(mr_iid, pipeline_id))')

    def migrate_to(self, dest_version):
        """Migrate the database to the selected version

        Args:
            dest_version (int): Version to migrate to
        """
        while True:
            version = self.get_schema_version()
            if version >= dest_version:
                break

            self.close()
            tools.write_file(f'{self.db_path}old.v{version}',
                             tools.read_file(self.db_path))

            version += 1
            tout.info(f'Update database to v{version}')
            self.open_it()
            if version == 1:
                self._create_v1()
            elif version == 2:
                self._create_v2()
            elif version == 3:
                self._create_v3()
            elif version == 4:
                self._create_v4()

            self.cur.execute('DELETE FROM schema_version')
            self.cur.execute(
                'INSERT INTO schema_version (version) VALUES (?)',
                (version,))
            self.commit()

    def get_schema_version(self):
        """Get the version of the database's schema

        Only returns 0 when the schema_version table genuinely does not
        exist (i.e. a fresh database).  Other errors (read-only, locked,
        corrupt) are re-raised so that migrate_to() does not accidentally
        destroy an existing database by re-running all migrations.

        Return:
            int: Database version, 0 means there is no data

        Raises:
            sqlite3.OperationalError: For errors other than a missing table
        """
        try:
            self.cur.execute('SELECT version FROM schema_version')
            return self.cur.fetchone()[0]
        except sqlite3.OperationalError as exc:
            if 'no such table' in str(exc):
                return 0
            raise

    def execute(self, query, parameters=()):
        """Execute a database query

        Args:
            query (str): Query string
            parameters (tuple): Parameters to pass

        Return:
            Cursor result
        """
        return self.cur.execute(query, parameters)

    def commit(self):
        """Commit changes to the database"""
        self.con.commit()

    def rollback(self):
        """Roll back changes to the database"""
        self.con.rollback()

    # source functions

    def source_get(self, name):
        """Get the last cherry-picked commit for a source branch

        Args:
            name (str): Source branch name

        Return:
            str: Commit hash, or None if not found
        """
        res = self.execute(
            'SELECT last_commit FROM source WHERE name = ?', (name,))
        rec = res.fetchone()
        if rec:
            return rec[0]
        return None

    def source_get_all(self):
        """Get all source branches and their last commits

        Return:
            list of tuple: (name, last_commit) pairs
        """
        res = self.execute('SELECT name, last_commit FROM source ORDER BY name')
        return res.fetchall()

    def source_set(self, name, commit):
        """Set the last cherry-picked commit for a source branch

        Args:
            name (str): Source branch name
            commit (str): Commit hash
        """
        self.execute(
            'UPDATE source SET last_commit = ? WHERE name = ?', (commit, name))
        if self.cur.rowcount == 0:
            self.execute(
                'INSERT INTO source (name, last_commit) VALUES (?, ?)',
                (name, commit))

    def source_get_id(self, name):
        """Get the id for a source branch

        Args:
            name (str): Source branch name

        Return:
            int: Source id, or None if not found
        """
        res = self.execute('SELECT id FROM source WHERE name = ?', (name,))
        rec = res.fetchone()
        if rec:
            return rec[0]
        return None

    # commit functions

    # pylint: disable-next=too-many-arguments
    def commit_add(self, chash, source_id, subject, author, status='pending',
                   mergereq_id=None):
        """Add a commit to the database

        Args:
            chash (str): Commit hash
            source_id (int): Source branch id
            subject (str): Commit subject line
            author (str): Commit author
            status (str): Status (pending, applied, skipped, conflict)
            mergereq_id (int): Merge request id (optional)
        """
        self.execute(
            'INSERT OR REPLACE INTO pcommit '
            '(chash, source_id, mergereq_id, subject, author, status) '
            'VALUES (?, ?, ?, ?, ?, ?)',
            (chash, source_id, mergereq_id, subject, author, status))

    def commit_get(self, chash):
        """Get a commit by hash

        Args:
            chash (str): Commit hash

        Return:
            tuple: (id, chash, source_id, mergereq_id, subject, author, status,
                   cherry_hash) or None if not found
        """
        res = self.execute(
            'SELECT id, chash, source_id, mergereq_id, subject, author, '
            'status, cherry_hash FROM pcommit WHERE chash = ?', 
            (chash,))
        return res.fetchone()

    def commit_get_by_source(self, source_id, status=None):
        """Get all commits for a source branch

        Args:
            source_id (int): Source branch id
            status (str): Optional status filter

        Return:
            list of tuple: Commit records
        """
        if status:
            res = self.execute(
                'SELECT id, chash, source_id, mergereq_id, subject, author, '
                'status, cherry_hash FROM pcommit '
                'WHERE source_id = ? AND status = ?',
                (source_id, status))
        else:
            res = self.execute(
                'SELECT id, chash, source_id, mergereq_id, subject, author, '
                'status, cherry_hash FROM pcommit WHERE source_id = ?',
                (source_id,))
        return res.fetchall()

    def commit_get_by_mergereq(self, mergereq_id):
        """Get all commits for a merge request

        Args:
            mergereq_id (int): Merge request id

        Return:
            list of tuple: Commit records
        """
        res = self.execute(
            'SELECT id, chash, source_id, mergereq_id, subject, author, '
            'status, cherry_hash FROM pcommit WHERE mergereq_id = ?',
            (mergereq_id,))
        return res.fetchall()

    def commit_set_status(self, chash, status, cherry_hash=None):
        """Update the status of a commit

        Args:
            chash (str): Commit hash
            status (str): New status
            cherry_hash (str): Hash of cherry-picked commit (optional)
        """
        if cherry_hash:
            self.execute(
                'UPDATE pcommit SET status = ?, cherry_hash = ? '
                'WHERE chash = ?',
                (status, cherry_hash, chash))
        else:
            self.execute(
                'UPDATE pcommit SET status = ? WHERE chash = ?',
                (status, chash))

    def commit_set_mergereq(self, chash, mergereq_id):
        """Set the merge request for a commit

        Args:
            chash (str): Commit hash
            mergereq_id (int): Merge request id
        """
        self.execute(
            'UPDATE pcommit SET mergereq_id = ? WHERE chash = ?',
            (mergereq_id, chash))

    def commit_delete(self, chash):
        """Delete a commit from the database

        Args:
            chash (str): Commit hash to delete
        """
        self.execute('DELETE FROM pcommit WHERE chash = ?', (chash,))

    # mergereq functions

    # pylint: disable-next=too-many-arguments
    def mergereq_add(self, source_id, branch_name, mr_id, status, url,
                     created_at):
        """Add a merge request to the database

        Args:
            source_id (int): Source branch id
            branch_name (str): Branch name for the MR
            mr_id (int): GitLab MR id
            status (str): Status (open, merged, closed)
            url (str): URL to the MR
            created_at (str): Creation timestamp
        """
        self.execute(
            'INSERT INTO mergereq '
            '(source_id, branch_name, mr_id, status, url, created_at) '
            'VALUES (?, ?, ?, ?, ?, ?)',
            (source_id, branch_name, mr_id, status, url, created_at))

    def mergereq_get(self, mr_id):
        """Get a merge request by GitLab MR id

        Args:
            mr_id (int): GitLab MR id

        Return:
            tuple: (id, source_id, branch_name, mr_id, status, url, created_at)
                   or None if not found
        """
        res = self.execute(
            'SELECT id, source_id, branch_name, mr_id, status, url, created_at '
            'FROM mergereq WHERE mr_id = ?', (mr_id,))
        return res.fetchone()

    def mergereq_get_by_source(self, source_id, status=None):
        """Get all merge requests for a source branch

        Args:
            source_id (int): Source branch id
            status (str): Optional status filter

        Return:
            list of tuple: Merge request records
        """
        if status:
            res = self.execute(
                'SELECT id, source_id, branch_name, mr_id, status, url, '
                'created_at FROM mergereq WHERE source_id = ? AND status = ?',
                (source_id, status))
        else:
            res = self.execute(
                'SELECT id, source_id, branch_name, mr_id, status, url, '
                'created_at FROM mergereq WHERE source_id = ?', (source_id,))
        return res.fetchall()

    def mergereq_set_status(self, mr_id, status):
        """Update the status of a merge request

        Args:
            mr_id (int): GitLab MR id
            status (str): New status
        """
        self.execute(
            'UPDATE mergereq SET status = ? WHERE mr_id = ?', (status, mr_id))

    # comment functions

    def comment_is_processed(self, mr_iid, comment_id):
        """Check if a comment has been processed

        Args:
            mr_iid (int): Merge request IID
            comment_id (int): Comment ID

        Return:
            bool: True if already processed
        """
        res = self.execute(
            'SELECT id FROM comment WHERE mr_iid = ? AND comment_id = ?',
            (mr_iid, comment_id))
        return res.fetchone() is not None

    def comment_mark_processed(self, mr_iid, comment_id):
        """Mark a comment as processed

        Args:
            mr_iid (int): Merge request IID
            comment_id (int): Comment ID
        """
        self.execute(
            'INSERT OR IGNORE INTO comment '
            '(mr_iid, comment_id, processed_at) VALUES (?, ?, ?)',
            (mr_iid, comment_id, datetime.now().isoformat()))

    def comment_get_processed(self, mr_iid):
        """Get all processed comment IDs for an MR

        Args:
            mr_iid (int): Merge request IID

        Return:
            list: List of comment IDs
        """
        res = self.execute(
            'SELECT comment_id FROM comment WHERE mr_iid = ?',
            (mr_iid,))
        return [row[0] for row in res.fetchall()]

    # pipeline_fix functions

    def pfix_count(self, mr_iid):
        """Count fix attempts for an MR

        A pipeline left alone because its failures also happen in other
        merge requests is not an attempt, so it does not count.

        Args:
            mr_iid (int): Merge request IID

        Return:
            int: Number of fix attempts
        """
        res = self.execute(
            'SELECT COUNT(*) FROM pipeline_fix '
            "WHERE mr_iid = ? AND status != 'shared'",
            (mr_iid,))
        return res.fetchone()[0]

    def pfix_add(self, mr_iid, pipeline_id, attempt, status):
        """Record a pipeline fix attempt

        Args:
            mr_iid (int): Merge request IID
            pipeline_id (int): Pipeline ID
            attempt (int): Attempt number
            status (str): Status ('success' or 'failure')
        """
        self.execute(
            'INSERT OR IGNORE INTO pipeline_fix '
            '(mr_iid, pipeline_id, attempt, status, created_at) '
            'VALUES (?, ?, ?, ?, ?)',
            (mr_iid, pipeline_id, attempt, status,
             datetime.now().isoformat()))

    def pfix_has(self, mr_iid, pipeline_id):
        """Check if a pipeline has already been handled

        Args:
            mr_iid (int): Merge request IID
            pipeline_id (int): Pipeline ID

        Return:
            bool: True if already handled
        """
        res = self.execute(
            'SELECT id FROM pipeline_fix '
            'WHERE mr_iid = ? AND pipeline_id = ?',
            (mr_iid, pipeline_id))
        return res.fetchone() is not None
