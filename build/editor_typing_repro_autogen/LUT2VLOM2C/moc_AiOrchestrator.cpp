/****************************************************************************
** Meta object code from reading C++ file 'AiOrchestrator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ai/AiOrchestrator.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AiOrchestrator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2ai14AiOrchestratorE_t {};
} // unnamed namespace

template <> constexpr inline auto ai::AiOrchestrator::qt_create_metaobjectdata<qt_meta_tag_ZN2ai14AiOrchestratorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ai::AiOrchestrator",
        "answerReady",
        "",
        "ai::AiOrchestrator::Answer",
        "answer",
        "retryProgress",
        "attempt",
        "totalAllowed",
        "errorPreview"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'answerReady'
        QtMocHelpers::SignalData<void(const ai::AiOrchestrator::Answer &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'retryProgress'
        QtMocHelpers::SignalData<void(int, int, const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { QMetaType::Int, 7 }, { QMetaType::QString, 8 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AiOrchestrator, qt_meta_tag_ZN2ai14AiOrchestratorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ai::AiOrchestrator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ai14AiOrchestratorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ai14AiOrchestratorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2ai14AiOrchestratorE_t>.metaTypes,
    nullptr
} };

void ai::AiOrchestrator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AiOrchestrator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->answerReady((*reinterpret_cast< std::add_pointer_t<ai::AiOrchestrator::Answer>>(_a[1]))); break;
        case 1: _t->retryProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< ai::AiOrchestrator::Answer >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AiOrchestrator::*)(const ai::AiOrchestrator::Answer & )>(_a, &AiOrchestrator::answerReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AiOrchestrator::*)(int , int , const QString & )>(_a, &AiOrchestrator::retryProgress, 1))
            return;
    }
}

const QMetaObject *ai::AiOrchestrator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ai::AiOrchestrator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ai14AiOrchestratorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ai::AiOrchestrator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void ai::AiOrchestrator::answerReady(const ai::AiOrchestrator::Answer & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ai::AiOrchestrator::retryProgress(int _t1, int _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
