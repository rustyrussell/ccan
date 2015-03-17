#ifndef NTDB_NO_FSYNC_H
#define NTDB_NO_FSYNC_H
/* Obey $TDB_NO_FSYNC, a bit like tdb does (only note our NTDB_NOSYNC
 * does less) */
#define MAYBE_NOSYNC (getenv("TDB_NO_FSYNC") ? NTDB_NOSYNC : 0)
#endif
