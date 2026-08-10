#include "mh4g_equipment_values.hpp"

#include "mh4g_ui_compat.hpp"

#include <QRegExp>
#include <QtMath>

#include <numeric>

int MH4GSharpnessResult::total() const
{
    return std::accumulate(lengths.begin(), lengths.end(), 0);
}

bool MH4GEquipmentValues::isWeaponType(int equipmentType)
{
    return equipmentType >= MH4G_Type::GSType && equipmentType <= MH4G_Type::CBType;
}

bool MH4GEquipmentValues::isRangedType(int equipmentType)
{
    return equipmentType == MH4G_Type::LBGType || equipmentType == MH4G_Type::HBGType ||
           equipmentType == MH4G_Type::BowType;
}

bool MH4GEquipmentValues::usesMeleeSharpness(int equipmentType)
{
    return isWeaponType(equipmentType) && !isRangedType(equipmentType);
}

double MH4GEquipmentValues::attackDisplayMultiplier(int equipmentType)
{
    switch (equipmentType)
    {
        case MH4G_Type::GSType: return 4.8;
        case MH4G_Type::SNSType: return 1.4;
        case MH4G_Type::HType: return 5.2;
        case MH4G_Type::LType: return 2.3;
        case MH4G_Type::LBGType: return 1.3;
        case MH4G_Type::HBGType: return 1.5;
        case MH4G_Type::LSType: return 3.3;
        case MH4G_Type::SAType: return 5.4;
        case MH4G_Type::GLType: return 2.3;
        case MH4G_Type::BowType: return 1.2;
        case MH4G_Type::DBType: return 1.4;
        case MH4G_Type::HHType: return 5.2;
        case MH4G_Type::IGType: return 3.1;
        case MH4G_Type::CBType: return 3.6;
        default: return 1.0;
    }
}

MH4GAttackResult MH4GEquipmentValues::attack(int equipmentType, std::uint8_t attackTier,
                                              std::uint8_t weaponSpecial,
                                              std::uint8_t upgrade,
                                              std::uint8_t honing)
{
    static const int normalRaw[] = {
        110, 120, 130, 140, 150, 160, 170, 170, 170, 180, 170,
        180, 190, 230, 210, 240, 250, 270, 290, 310, 340,
    };

    MH4GAttackResult result;
    if (!isWeaponType(equipmentType)) return result;

    if (attackTier <= 0x14)
    {
        result.baseTrueRaw = normalRaw[attackTier];
    }
    else if (!isRangedType(equipmentType) && attackTier == 0x5B)
    {
        result.baseTrueRaw = 355;
    }
    else if (isRangedType(equipmentType) && attackTier == 0x1C)
    {
        result.baseTrueRaw = 255;
    }
    else
    {
        return result;
    }

    if ((equipmentType == MH4G_Type::GSType || equipmentType == MH4G_Type::HType) &&
        weaponSpecial <= 2)
        result.weaponBonus = weaponSpecial * 10;

    // MH4G stores the relic upgrade as a separate 0-3 stage. Public editors
    // expose the stages but do not publish the panel formula, so use the
    // regular ten-true-raw progression as an explicit reference estimate.
    // Keeping this isolated makes replacing it with a verified table trivial.
    const bool upgradeKnown = upgrade <= 3;
    if (upgradeKnown)
    {
        result.upgradeBonus = upgrade * 10;
        result.upgradeBonusEstimated = upgrade != 0;
    }

    // MH4U damage calculators consistently model Attack Honing as +20 true
    // raw. Defense and Life honing do not alter the displayed weapon attack.
    const bool honingKnown = honing == 0x00 || honing == 0x40 ||
                             honing == 0x80 || honing == 0xC0;
    if (honing == 0x40) result.honingBonus = 20;

    result.known = true;
    result.modifiersKnown = upgradeKnown && honingKnown;
    result.trueRaw = result.baseTrueRaw + result.weaponBonus +
                     result.upgradeBonus + result.honingBonus;
    result.multiplier = attackDisplayMultiplier(equipmentType);
    result.panelValue = qRound(result.trueRaw * result.multiplier);
    return result;
}

MH4GSharpnessResult MH4GEquipmentValues::sharpness(std::uint8_t sharpnessCode)
{
    // Reconstructed from the 23 embedded sharpness bars in mikewii/MH4U-Editor.
    // The source bitmaps use one displayed pixel per five sharpness points after
    // removing their 2x scaling and border. Values 0x00-0x15 are direct table
    // indices. MH4G value 0xDA is provisionally paired with that editor's sole
    // red/purple overflow bar (stored as 0x9E in its MH4U save writer).
    static const std::array<std::array<int, 7>, 22> normal = {{
        {{150, 20,  80,   0,   0,   0,  0}},
        {{200, 10,  20,  20,   0,   0,  0}},
        {{ 30, 30, 170,  20,   0,   0,  0}},
        {{ 30, 20, 175,  75,   0,   0,  0}},
        {{ 40, 60,  80, 120,   0,   0,  0}},
        {{ 60, 30, 100, 100,  10,   0,  0}},
        {{100, 70,  80,  40,  10,   0,  0}},
        {{ 40,160,  30,  20,  50,   0,  0}},
        {{ 30, 70,  70,  20,  60,   0,  0}},
        {{ 30, 70,  70,  20, 100,  10,  0}},
        {{ 50, 70,  80, 100,  75,  20,  0}},
        {{ 60, 40,  50, 160,  40,   0,  0}},
        {{120, 20,  40,  90,  30,   0,  0}},
        {{190, 20,  60,  80,   0,   0,  0}},
        {{ 30, 60, 160,  50,   0,   0,  0}},
        {{ 10, 50, 190,   0,   0,   0,  0}},
        {{ 30, 20, 150,   0,   0,   0,  0}},
        {{100, 60,  40,  70,  50,  30,  0}},
        {{ 30, 70,  70,  20, 100, 105,  0}},
        {{ 50, 70,  80, 100,  75,  20,  0}},
        {{145, 10,  20, 150,  20,  50,  0}},
        {{ 95, 80,  40,  60,  70,  80, 25}},
    }};

    MH4GSharpnessResult result;
    if (sharpnessCode <= 0x15)
    {
        result.known = true;
        result.lengths = normal[sharpnessCode];
    }
    else if (sharpnessCode == 0xDA)
    {
        result.known = true;
        result.provisional = true;
        result.lengths = {{235, 0, 0, 0, 0, 0, 215}};
    }
    return result;
}

MH4GLookupNumber MH4GEquipmentValues::parseLookupNumber(const QString &display)
{
    MH4GLookupNumber result;
    const QString trimmed = display.trimmed();
    result.awakened = trimmed.startsWith('(') && trimmed.endsWith(')');
    QRegExp number("(-?[0-9]+)");
    if (number.indexIn(trimmed) < 0) return result;
    bool ok = false;
    const int value = number.cap(1).toInt(&ok);
    if (!ok) return result;
    result.known = true;
    result.value = value;
    return result;
}
