import os
from dotenv import load_dotenv
import logging
import psycopg
from psycopg.connection import Connection as PGConnection

load_dotenv()

VALID_SYNC_MODES = {"remote_write", "on", "remote_apply"}
DEFAULT_DSN_PARAMS = {
    "host": os.environ.get("PGHOST", "localhost"),
    "port": os.environ.get("PGPORT", "5432"),
    "dbname": os.environ.get("PGDATABASE", "postgres"),
    "user": os.environ.get("PGUSER", "postgres"),
    "password": os.environ.get("PGPASSWORD", ""),
}


def get_connection(
    host: str = None,
    port: str = None,
    dbname: str = None,
    user: str = None,
    password: str = None,
    connect_timeout: int = 5,
) -> PGConnection:
    params = {
        "host": host or DEFAULT_DSN_PARAMS["host"],
        "port": port or DEFAULT_DSN_PARAMS["port"],
        "dbname": dbname or DEFAULT_DSN_PARAMS["dbname"],
        "user": user or DEFAULT_DSN_PARAMS["user"],
        "password": password or DEFAULT_DSN_PARAMS["password"],
        "connect_timeout": connect_timeout,
        "autocommit": True,
    }

    try:
        conn = psycopg.connect(**params)
    except psycopg.OperationalError as e:
        raise
    return conn


def set_synchronous_commit(conn: PGConnection, mode: str) -> None:
    if mode not in VALID_SYNC_MODES:
        raise ValueError(
            f"Invalid mode synchronous_commit: '{mode}'. "
            f"Allowed: {sorted(VALID_SYNC_MODES)}"
        )

    with conn.cursor() as cur:
        cur.execute(f"SET synchronous_commit = {mode};")


class DBConnection:
    """
    Пример:
        with DBConnection(host="127.0.0.1", synchronous_commit="remote_apply") as conn:
            cur = conn.cursor()
            cur.execute(SQL('query'))
            ...
    """

    def __init__(self, synchronous_commit: str = None, **connect_kwargs):
        self._connect_kwargs = connect_kwargs
        self._synchronous_commit = synchronous_commit
        self.conn: PGConnection = None

    def __enter__(self) -> PGConnection:
        self.conn = get_connection(**self._connect_kwargs)
        if self._synchronous_commit:
            set_synchronous_commit(self.conn, self._synchronous_commit)
        return self.conn

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.conn is not None and not self.conn.closed:
            self.conn.close()
        return False

