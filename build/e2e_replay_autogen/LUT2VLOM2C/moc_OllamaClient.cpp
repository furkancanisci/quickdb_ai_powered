/****************************************************************************
** Meta object code from reading C++ file 'OllamaClient.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ai/OllamaClient.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'OllamaClient.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN2ai12OllamaClientE_t {};
} // unnamed namespace

template <> constexpr inline auto ai::OllamaClient::qt_create_metaobjectdata<qt_meta_tag_ZN2ai12OllamaClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ai::OllamaClient",
        "availabilityChecked",
        "",
        "available",
        "installedModels",
        "generationFinished",
        "ok",
        "rawText",
        "error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'availabilityChecked'
        QtMocHelpers::SignalData<void(bool, const QStringList &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 3 }, { QMetaType::QStringList, 4 },
        }}),
        // Signal 'generationFinished'
        QtMocHelpers::SignalData<void(bool, const QString &, const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 }, { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OllamaClient, qt_meta_tag_ZN2ai12OllamaClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ai::OllamaClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ai12OllamaClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ai12OllamaClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN2ai12OllamaClientE_t>.metaTypes,
    nullptr
} };

void ai::OllamaClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OllamaClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->availabilityChecked((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2]))); break;
        case 1: _t->generationFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OllamaClient::*)(bool , const QStringList & )>(_a, &OllamaClient::availabilityChecked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (OllamaClient::*)(bool , const QString & , const QString & )>(_a, &OllamaClient::generationFinished, 1))
            return;
    }
}

const QMetaObject *ai::OllamaClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ai::OllamaClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN2ai12OllamaClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ai::OllamaClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void ai::OllamaClient::availabilityChecked(bool _t1, const QStringList & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void ai::OllamaClient::generationFinished(bool _t1, const QString & _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
