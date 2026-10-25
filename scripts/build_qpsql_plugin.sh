#!/usr/bin/env bash
# Build the QPSQL Qt driver plugin and drop it into QuickDb.app's PlugIns dir.
#
# Homebrew's qtbase ships only libqsqlite, so PostgreSQL support needs the
# stock Qt psql driver compiled against the local Qt + libpq. This is the
# risky spike from the plan: if it fails we DO NOT break the app - QuickDb
# just won't list QPSQL and degrades with its existing "driver unavailable"
# notice. Run:  scripts/build_qpsql_plugin.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/third_party/qsqlpsql"
BLD="${SRC}/build"
APP="${ROOT}/build/QuickDb.app"
QT_PREFIX="${QT_PREFIX:-/opt/homebrew/opt/qt}"
LIBPQ="${LIBPQ_PREFIX:-/opt/homebrew/opt/libpq}"

echo "==> Configuring QPSQL plugin build"
cmake -S "${SRC}" -B "${BLD}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX};${LIBPQ}"

echo "==> Compiling"
cmake --build "${BLD}" --config Release -j4

DYLIB="${BLD}/libqsqlpsql.dylib"
if [[ ! -f "${DYLIB}" ]]; then
    echo "!! Expected artifact not found: ${DYLIB}" >&2
    exit 1
fi

# libpq is dlopen'd at runtime; make sure the driver can find the .5 dylib.
LIBPQ_DIR="${LIBPQ}/lib"
if [[ -d "${LIBPQ_DIR}" ]]; then
    echo "==> Fixing runtime libpq lookup (${LIBPQ_DIR})"
    install_name_tool -add_rpath "${LIBPQ_DIR}" "${DYLIB}" 2>/dev/null || true
fi

# Install ONLY inside the workspace (never touch the Homebrew Qt tree): the
# app bundle's PlugIns dir (for the GUI) plus a build/sqldrivers dir that the
# headless tests reach via QT_PLUGIN_PATH.
echo "==> Installing plugin"
for DEST in \
    "${APP}/Contents/PlugIns/sqldrivers" \
    "${ROOT}/build/sqldrivers"; do
    mkdir -p "${DEST}"
    cp -f "${DYLIB}" "${DEST}/"
    echo "   -> ${DEST}/libqsqlpsql.dylib"
done

echo "==> Done. QPSQL driver should now appear in QSqlDatabase::drivers()."
echo "    (headless tests: export QT_PLUGIN_PATH=${ROOT}/build)"
