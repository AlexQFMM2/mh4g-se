#include "equipment_validator.hpp"

#include "game_data_repository.hpp"

equipment_validation_t EquipmentValidator::validate(const equipment_t &record,
                                                      save_format_e, int gender)
{
    equipment_validation_t result;
    const int type = record[0];
    const int id = record[2] | (record[3] << 8);
    if (type == MH4G_Type::NoneType && id == 0) return result;
    if (type == MH4G_Type::CharmType)
    {
        if (!GameDataRepository::instance().charmClassExists(id))
        {
            result.status = EquipmentInvalid;
            result.diagnostics << QString::fromUtf8("护石品级 ID 无法解析");
        }
        return result;
    }
    const loadout_candidate_t candidate = GameDataRepository::instance().candidate(type, id);
    if (!candidate.found)
    {
        result.status = EquipmentInvalid;
        result.diagnostics << QString::fromUtf8("装备类型或 ID 无法解析");
        return result;
    }
    if (type >= MH4G_Type::ChestType && type <= MH4G_Type::HeadType &&
        gender >= 0 && candidate.gender > 0 && candidate.gender != gender + 1)
    {
        result.status = EquipmentUnknown;
        result.diagnostics << QString::fromUtf8("与当前配装性别不适用");
    }
    return result;
}
