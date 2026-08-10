#ifndef MH4G_EQUIPMENT_VALUES_HPP
#define MH4G_EQUIPMENT_VALUES_HPP

#include <QString>

#include <array>
#include <cstdint>

struct MH4GAttackResult
{
    bool known = false;
    bool modifiersKnown = false;
    bool upgradeBonusEstimated = false;
    int baseTrueRaw = 0;
    int weaponBonus = 0;
    int upgradeBonus = 0;
    int honingBonus = 0;
    std::uint8_t honingMode = 0;
    std::uint8_t honingExtraBits = 0;
    int trueRaw = 0;
    double multiplier = 1.0;
    int panelValue = 0;
};

struct MH4GLookupNumber
{
    bool known = false;
    bool awakened = false;
    int value = 0;
};

struct MH4GSharpnessResult
{
    bool known = false;
    bool provisional = false;
    std::array<int, 7> lengths{{0, 0, 0, 0, 0, 0, 0}};

    int total() const;
};

class MH4GEquipmentValues
{
public:
    static bool isWeaponType(int equipmentType);
    static bool isRangedType(int equipmentType);
    static bool usesMeleeSharpness(int equipmentType);
    static double attackDisplayMultiplier(int equipmentType);
    static MH4GAttackResult attack(int equipmentType, std::uint8_t attackTier,
                                   std::uint8_t weaponSpecial,
                                   std::uint8_t upgrade = 0,
                                   std::uint8_t honing = 0);
    static MH4GSharpnessResult sharpness(std::uint8_t sharpnessCode);
    static MH4GLookupNumber parseLookupNumber(const QString &display);
};

#endif
