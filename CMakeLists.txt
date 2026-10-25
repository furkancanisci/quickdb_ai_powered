cmake_minimum_required(VERSION 3.21)
project(QuickDb VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Test Charts Network Xml)

# Sources for the widget-free core + ai logic layers (shared by app and tests).
set(CORE_SOURCES
    src/core/DatabaseManager.h
    src/core/DatabaseManager.cpp
    src/core/io/Exporters.h
    src/core/io/Exporters.cpp
    src/core/io/Importers.h
    src/core/io/Importers.cpp
)

set(AI_SOURCES
    src/ai/SchemaContext.h
    src/ai/SchemaContext.cpp
    src/ai/OllamaClient.h
    src/ai/OllamaClient.cpp
    src/ai/RuleBasedNlu.h
    src/ai/RuleBasedNlu.cpp
    src/ai/DataInspector.h
    src/ai/DataInspector.cpp
    src/ai/AiOrchestrator.h
    src/ai/AiOrchestrator.cpp
)

set(UI_SOURCES
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/core/web/DashboardServer.h
    src/core/web/DashboardServer.cpp
    src/ui/ResultTab.h
    src/ui/ResultTab.cpp
    src/ui/SqlKeywords.h
    src/ui/SqlHighlighter.h
    src/ui/SqlHighlighter.cpp
    src/ui/SchemaTreeModel.h
    src/ui/SchemaTreeModel.cpp
    src/ui/QueryEditor.h
    src/ui/QueryEditor.cpp
    src/ui/SelectQueryModel.h
    src/ui/SelectQueryModel.cpp
    src/ui/ErDiagramView.h
    src/ui/ErDiagramView.cpp
    src/ui/Dialogs/CreateTableDialog.h
    src/ui/Dialogs/CreateTableDialog.cpp
    src/ui/Dialogs/AddColumnDialog.h
    src/ui/Dialogs/AddColumnDialog.cpp
    src/ui/Dialogs/ConnectionDialog.h
    src/ui/Dialogs/ConnectionDialog.cpp
    src/ui/Dialogs/CreateViewDialog.h
    src/ui/Dialogs/CreateViewDialog.cpp
    src/ui/Dialogs/CreateIndexDialog.h
    src/ui/Dialogs/CreateIndexDialog.cpp
    src/ui/Dialogs/ExportDialog.h
    src/ui/Dialogs/ExportDialog.cpp
    src/ui/Dialogs/ImportDialog.h
    src/ui/Dialogs/ImportDialog.cpp
    src/ui/AiAssistantPanel.h
    src/ui/AiAssistantPanel.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)

add_executable(QuickDb MACOSX_BUNDLE
    src/main.cpp
    ${CORE_SOURCES}
    ${AI_SOURCES}
    ${UI_SOURCES}
)
target_include_directories(QuickDb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(QuickDb PRIVATE Qt6::Widgets Qt6::Sql Qt6::Charts Qt6::Network Qt6::Xml)

# Console smoke test for the core layer (no GUI).
add_executable(smoke_test
    test/smoke_main.cpp
    ${CORE_SOURCES}
)
target_include_directories(smoke_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(smoke_test PRIVATE Qt6::Sql Qt6::Core Qt6::Xml)

# File I/O round-trip test for the export/import layer (no GUI, no database).
add_executable(io_test
    test/io_main.cpp
    ${CORE_SOURCES}
)
target_include_directories(io_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(io_test PRIVATE Qt6::Core Qt6::Xml Qt6::Sql Qt6::Test)

# Offline AI logic test (rule engine + inspector; no network, no GUI widgets).
add_executable(ai_test
    test/ai_main.cpp
    ${CORE_SOURCES}
    ${AI_SOURCES}
)
target_include_directories(ai_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ai_test PRIVATE Qt6::Sql Qt6::Network Qt6::Core Qt6::Xml)

# Headless UI integration test (run with QT_QPA_PLATFORM=offscreen).
add_executable(ui_test
    test/ui_main.cpp
    ${CORE_SOURCES}
    ${AI_SOURCES}
    ${UI_SOURCES}
)
target_include_directories(ui_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ui_test PRIVATE Qt6::Widgets Qt6::Sql Qt6::Test Qt6::Charts Qt6::Network Qt6::Xml)

# Live end-to-end AI driver against a real DB + local Ollama model (no GUI).
# Run manually: ./build/e2e_ai_test samples/company_analytics.db
add_executable(e2e_ai_test
    test/e2e_main.cpp
    ${CORE_SOURCES}
    ${AI_SOURCES}
)
target_include_directories(e2e_ai_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(e2e_ai_test PRIVATE Qt6::Sql Qt6::Network Qt6::Core Qt6::Xml)

# One-shot replay of an arbitrary question file through the live pipeline.
add_executable(e2e_replay
    test/replay_main.cpp
    ${CORE_SOURCES}
    ${AI_SOURCES}
)
target_include_directories(e2e_replay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(e2e_replay PRIVATE Qt6::Sql Qt6::Network Qt6::Core Qt6::Xml)

# PostgreSQL integration test. Skips (returns 0) when no QPSQL driver or no
# local server is reachable, so it is safe to run in every environment:
#   scripts/build_qpsql_plugin.sh && scripts/setup_local_pg.sh
#   QT_PLUGIN_PATH=build ./build/pg_e2e_test
add_executable(pg_e2e_test
    test/pg_e2e_main.cpp
    ${CORE_SOURCES}
)
target_include_directories(pg_e2e_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pg_e2e_test PRIVATE Qt6::Sql Qt6::Core)
