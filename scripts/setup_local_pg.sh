#!/usr/bin/env bash
# Spin up a throwaway, project-local PostgreSQL instance for pg_e2e_test.
#
# It initialises a fresh cluster under pgdata_test/ (NOT touching any system
# Postgres), starts it on port 5433, creates the `quickdb_test` database and
# loads samples/company_analytics_pg.sql. Re-running is idempotent: an existing
# running cluster is reused, the schema is reloaded from scratch.
#
#   scripts/setup_local_pg.sh            # start + seed
#   scripts/setup_local_pg.sh stop        # stop the cluster
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGDATA="${ROOT}/pgdata_test"
PORT="${PGPORT_TEST:-5433}"
DB="quickdb_test"
SOCK="/tmp"
LOG="${ROOT}/pgdata_test.log"

# Locate the Homebrew postgres binaries (prefer @15, fall back to @14 / PATH).
pg_bin() {
    for c in /opt/homebrew/opt/postgresql@15/bin \
             /opt/homebrew/opt/postgresql@14/bin \
             /usr/local/opt/postgresql@15/bin; do
        [[ -x "${c}/initdb" ]] && { echo "${c}"; return 0; }
    done
    command -v initdb >/dev/null 2>&1 && { dirname "$(command -v initdb)"; return 0; }
    return 1
}
BIN="$(pg_bin)" || { echo "!! postgres initdb not found; brew install postgresql@15" >&2; exit 1; }
PGCTL="${BIN}/pg_ctl"
PSQL="${BIN}/psql"

if [[ "${1:-}" == "stop" ]]; then
    if [[ -d "${PGDATA}" ]]; then
        "${PGCTL}" -D "${PGDATA}" stop -m fast 2>/dev/null || true
        echo "==> Stopped test cluster."
    fi
    exit 0
fi

echo "==> Using postgres binaries: ${BIN}"

# 1. initdb (once).
if [[ ! -f "${PGDATA}/PG_VERSION" ]]; then
    echo "==> initdb into ${PGDATA}"
    "${BIN}/initdb" -D "${PGDATA}" -U postgres --auth=trust >/dev/null
fi

# 2. start (or reuse).
if "${PGCTL}" -D "${PGDATA}" status >/dev/null 2>&1; then
    echo "==> Cluster already running on port ${PORT}."
else
    echo "==> Starting cluster on port ${PORT}"
    "${PGCTL}" -D "${PGDATA}" -o "-p ${PORT} -k ${SOCK} -h 127.0.0.1" -l "${LOG}" start
    # wait for readiness
    for _ in $(seq 1 30); do
        "${PSQL}" -h 127.0.0.1 -p "${PORT}" -U postgres -c 'SELECT 1' >/dev/null 2>&1 && break
        sleep 0.5
    done
fi

# 3. (re)create the database.
if ! "${PSQL}" -h 127.0.0.1 -p "${PORT}" -U postgres -l | grep -q "${DB}"; then
    echo "==> Creating database ${DB}"
    "${PSQL}" -h 127.0.0.1 -p "${PORT}" -U postgres -c "CREATE DATABASE ${DB};" >/dev/null
fi

# 4. seed schema + data.
echo "==> Loading samples/company_analytics_pg.sql"
"${PSQL}" -h 127.0.0.1 -p "${PORT}" -U postgres -d "${DB}" -q -v ON_ERROR_STOP=1 \
    -f "${ROOT}/samples/company_analytics_pg.sql"

echo "==> Test PostgreSQL ready:"
echo "    host=127.0.0.1 port=${PORT} db=${DB} user=postgres (trust)"
echo "    Run: QT_PLUGIN_PATH=${ROOT}/build ${ROOT}/build/pg_e2e_test"
