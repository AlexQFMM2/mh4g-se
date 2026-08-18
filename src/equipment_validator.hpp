#ifndef MH4G_EQUIPMENT_VALIDATOR_HPP
#define MH4G_EQUIPMENT_VALIDATOR_HPP

#include "mh4g_ui_compat.hpp"

#include <QString>
#include <QStringList>

enum equipment_validation_status_e
{
    EquipmentValid = 0,
    EquipmentUnknown = 1,
    EquipmentInvalid = 2
};
typedef equipment_validation_status_e equipment_validity_e;

struct equipment_validation_t
{
    equipment_validation_status_e status;
    QStringList diagnostics;
    equipment_validation_t() : status(EquipmentValid) {}
    QString details() const { return diagnostics.join(QString::fromUtf8("；")); }
    QString statusText() const
    {
        return status == EquipmentValid ? QString::fromUtf8("结构正常") :
               status == EquipmentInvalid ? QString::fromUtf8("结构异常") : QString::fromUtf8("需人工确认");
    }
};

class EquipmentValidator
{
public:
    static equipment_validation_t validate(const equipment_t &record,
                                           save_format_e = SAVE_FORMAT_UNKNOWN,
                                           int gender = -1);
};

#endif
