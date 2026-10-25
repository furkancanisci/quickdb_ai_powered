/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/MainWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN2ui10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto ui::MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN2ui10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ui::MainWindow",
        "newDatabase",
        "",
        "openDatabase",
        "closeDatabase",
        "openConnectionDialog",
        "executeSql",
        "refreshSchema",
        "explainCurrentQuery",
        "formatCurrentSql",
        "createTableViaDialog",
        "addColumnViaDialog",
        "dropSelectedTable",
        "createViewViaDialog",
        "createIndexViaDialogForTable",
        "table",
        "showObjectDdl",
        "objectName",
        "dropTreeObject",
        "openViewTab",
        "view",
        "exportActiveResult",
        "exportSelectedTable",
        "importFile",
        "onTreeDoubleClicked",
        "QModelIndex",
        "index",
        "onTreeContextMenu",
        "pos",
        "addRowToTable",
        "deleteCurrentRow",
        "saveTableChanges",
        "revertTableChanges",
        "onResultContextMenu",
        "connectToPath",
        "filePath",
        "create",
        "onAiInsertSql",
        "sql",
        "onAiPresentResult",
        "ai::TableData",
        "data",
        "source",
        "message",
        "updateAiBadge",
        "available",
        "models",
        "openTableTab",
        "onTabCloseRequested"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'newDatabase'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openDatabase'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'closeDatabase'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openConnectionDialog'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'executeSql'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'refreshSchema'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'explainCurrentQuery'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'formatCurrentSql'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'createTableViaDialog'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'addColumnViaDialog'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'dropSelectedTable'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'createViewViaDialog'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'createIndexViaDialogForTable'
        QtMocHelpers::SlotData<void(const QString &)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'showObjectDdl'
        QtMocHelpers::SlotData<void(const QString &)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Slot 'dropTreeObject'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openViewTab'
        QtMocHelpers::SlotData<void(const QString &)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 20 },
        }}),
        // Slot 'exportActiveResult'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'exportSelectedTable'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'importFile'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTreeDoubleClicked'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 25, 26 },
        }}),
        // Slot 'onTreeContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 28 },
        }}),
        // Slot 'addRowToTable'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'deleteCurrentRow'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'saveTableChanges'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'revertTableChanges'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onResultContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 28 },
        }}),
        // Slot 'connectToPath'
        QtMocHelpers::SlotData<void(const QString &, bool)>(34, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 35 }, { QMetaType::Bool, 36 },
        }}),
        // Slot 'onAiInsertSql'
        QtMocHelpers::SlotData<void(const QString &)>(37, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 38 },
        }}),
        // Slot 'onAiPresentResult'
        QtMocHelpers::SlotData<void(const QString &, const ai::TableData &, const QString &, const QString &)>(39, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 38 }, { 0x80000000 | 40, 41 }, { QMetaType::QString, 42 }, { QMetaType::QString, 43 },
        }}),
        // Slot 'updateAiBadge'
        QtMocHelpers::SlotData<void(bool, const QStringList &)>(44, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 45 }, { QMetaType::QStringList, 46 },
        }}),
        // Method 'openTableTab'
        QtMocHelpers::MethodData<void(const QString &)>(47, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Method 'onTabCloseRequested'
        QtMocHelpers::MethodData<void(int)>(48, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 26 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN2ui10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ui::MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ui10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ui10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2ui10MainWindowE_t>.metaTypes,
    nullptr
} };

void ui::MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->newDatabase(); break;
        case 1: _t->openDatabase(); break;
        case 2: _t->closeDatabase(); break;
        case 3: _t->openConnectionDialog(); break;
        case 4: _t->executeSql(); break;
        case 5: _t->refreshSchema(); break;
        case 6: _t->explainCurrentQuery(); break;
        case 7: _t->formatCurrentSql(); break;
        case 8: _t->createTableViaDialog(); break;
        case 9: _t->addColumnViaDialog(); break;
        case 10: _t->dropSelectedTable(); break;
        case 11: _t->createViewViaDialog(); break;
        case 12: _t->createIndexViaDialogForTable((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->showObjectDdl((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->dropTreeObject(); break;
        case 15: _t->openViewTab((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->exportActiveResult(); break;
        case 17: _t->exportSelectedTable(); break;
        case 18: _t->importFile(); break;
        case 19: _t->onTreeDoubleClicked((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 20: _t->onTreeContextMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 21: _t->addRowToTable(); break;
        case 22: _t->deleteCurrentRow(); break;
        case 23: _t->saveTableChanges(); break;
        case 24: _t->revertTableChanges(); break;
        case 25: _t->onResultContextMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 26: _t->connectToPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 27: _t->onAiInsertSql((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 28: _t->onAiPresentResult((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<ai::TableData>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 29: _t->updateAiBadge((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2]))); break;
        case 30: _t->openTableTab((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 31: _t->onTabCloseRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *ui::MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ui::MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ui10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int ui::MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 32)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 32;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 32)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 32;
    }
    return _id;
}
QT_WARNING_POP
