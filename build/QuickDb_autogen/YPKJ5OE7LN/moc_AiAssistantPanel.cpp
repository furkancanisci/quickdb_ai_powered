/****************************************************************************
** Meta object code from reading C++ file 'AiAssistantPanel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/AiAssistantPanel.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AiAssistantPanel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2ui16AiAssistantPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto ui::AiAssistantPanel::qt_create_metaobjectdata<qt_meta_tag_ZN2ui16AiAssistantPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ui::AiAssistantPanel",
        "insertSqlRequested",
        "",
        "sql",
        "presentResult",
        "ai::TableData",
        "data",
        "source",
        "message",
        "askQuestion",
        "onAnswer",
        "ai::AiOrchestrator::Answer",
        "answer"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'insertSqlRequested'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'presentResult'
        QtMocHelpers::SignalData<void(const QString &, const ai::TableData &, const QString &, const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { 0x80000000 | 5, 6 }, { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
        }}),
        // Slot 'askQuestion'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onAnswer'
        QtMocHelpers::SlotData<void(const ai::AiOrchestrator::Answer &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 12 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AiAssistantPanel, qt_meta_tag_ZN2ui16AiAssistantPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ui::AiAssistantPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QTabWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ui16AiAssistantPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ui16AiAssistantPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2ui16AiAssistantPanelE_t>.metaTypes,
    nullptr
} };

void ui::AiAssistantPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AiAssistantPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->insertSqlRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->presentResult((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<ai::TableData>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 2: _t->askQuestion(); break;
        case 3: _t->onAnswer((*reinterpret_cast< std::add_pointer_t<ai::AiOrchestrator::Answer>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< ai::AiOrchestrator::Answer >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AiAssistantPanel::*)(const QString & )>(_a, &AiAssistantPanel::insertSqlRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AiAssistantPanel::*)(const QString & , const ai::TableData & , const QString & , const QString & )>(_a, &AiAssistantPanel::presentResult, 1))
            return;
    }
}

const QMetaObject *ui::AiAssistantPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ui::AiAssistantPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ui16AiAssistantPanelE_t>.strings))
        return static_cast<void*>(this);
    return QTabWidget::qt_metacast(_clname);
}

int ui::AiAssistantPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QTabWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void ui::AiAssistantPanel::insertSqlRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ui::AiAssistantPanel::presentResult(const QString & _t1, const ai::TableData & _t2, const QString & _t3, const QString & _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3, _t4);
}
QT_WARNING_POP
