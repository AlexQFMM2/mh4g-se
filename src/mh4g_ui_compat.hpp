#ifndef MH4G_UI_COMPAT_HPP
#define MH4G_UI_COMPAT_HPP

#include "mh4g.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#define EQUIPMENT_SIZE MH4GSave::EquipmentSize
#define cdelete(ptr) do { delete (ptr); (ptr) = NULL; } while (0)

typedef struct item_t
{
    std::uint16_t id;
    std::uint16_t count;
} item_t;

typedef std::uint8_t equipment_t[EQUIPMENT_SIZE];
typedef std::uint16_t name_t[12];

typedef struct armor_t
{
    equipment_t raw;
    std::uint8_t equipmentType;
    std::uint8_t upgradeLevel;
    std::uint16_t identifier;
    std::uint16_t firstJewelIdentifier;
    std::uint16_t secondJewelIdentifier;
    std::uint16_t thirdJewelIdentifier;
} armor_t;

typedef struct weapon_t
{
    equipment_t raw;
    std::uint8_t equipmentType;
    std::uint8_t levelOrModification;
    std::uint16_t identifier;
    std::uint16_t firstJewelIdentifier;
    std::uint16_t secondJewelIdentifier;
    std::uint16_t thirdJewelIdentifier;
} weapon_t;

typedef struct charm_t
{
    equipment_t raw;
    std::uint8_t equipmentType;
    std::uint8_t slotsCount;
    std::uint16_t identifier;
    std::uint16_t firstSkillIdentifier;
    std::int16_t firstSkillValue;
    std::uint16_t secondSkillIdentifier;
    std::int16_t secondSkillValue;
    std::uint16_t firstJewelIdentifier;
    std::uint16_t secondJewelIdentifier;
    std::uint16_t thirdJewelIdentifier;
} charm_t;

typedef struct save_t
{
    std::uint8_t sex;
    std::uint8_t face;
    std::uint8_t hair;
    name_t name;
    std::uint32_t money;
    std::uint8_t voice;
    item_t chest[14][100];
    equipment_t box[15][100];
    std::uint32_t mogapoint;
} save_t;

namespace MH4G_Type
{
enum equipment_type_e
{
    NoneType = 0,
    ChestType = 1,
    ArmsType = 2,
    WaistType = 3,
    LegsType = 4,
    HeadType = 5,
    CharmType = 6,
    GSType = 7,
    SNSType = 8,
    HType = 9,
    LType = 10,
    LBGType = 11,
    HBGType = 12,
    LSType = 13,
    SAType = 14,
    GLType = 15,
    BowType = 16,
    DBType = 17,
    HHType = 18,
    IGType = 19,
    CBType = 20,
};

enum equipment_subtype_e
{
    NoneSubtype = 0,
    ArmorSubtype = 1,
    CharmSubtype = 2,
    WeaponSubtype = 3,
};
}

namespace MH3U_Type = MH4G_Type;
typedef MH4G_Type::equipment_type_e equipment_type_e;
typedef MH4G_Type::equipment_subtype_e equipment_subtype_e;

class MH4G_UI_SaveAdapter
{
public:
    MH4G_UI_SaveAdapter();
    ~MH4G_UI_SaveAdapter();
    bool loaded() const;
    bool load(const std::string &input);
    bool save(const std::string &output);
    std::string lastError() const;
    std::string formatName() const;
    std::uint32_t nameSize() const;
    save_t *savedata;

private:
    void importModel();
    void exportModel();
    MH4GSave m_save;
};

using MH3U_SE = MH4G_UI_SaveAdapter;

typedef struct dataitem_t
{
    std::uint32_t count;
    std::string identifier;
    std::string name;
    std::string english;
    std::string source;
    std::int32_t rarity;
    bool isRelic;
} dataitem_t;

typedef std::vector<dataitem_t> dataset_t;

typedef struct lookupitem_t
{
    std::string domain;
    std::uint8_t equipmentType;
    std::string variant;
    std::uint8_t value;
    std::string identifier;
    std::string english;
    std::string source;
} lookupitem_t;

typedef std::vector<lookupitem_t> lookup_dataset_t;

enum lang_t
{
    LANG_NONE = 0,
    LANG_EN = 1,
    LANG_CN = 2,
};

class MH4G_UI_DataAdapter
{
public:
    static bool readData(lang_t lang);
    static bool deleteData();
    static lang_t lang();
    static const dataset_t *items();
    static const dataset_t *skills();
    static const dataset_t *jewels();
    static const dataset_t *equipmentTypes();
    static const dataset_t *equipment(std::uint8_t type);
    static const dataset_t *chestArmors();
    static const dataset_t *armsArmors();
    static const dataset_t *waistArmors();
    static const dataset_t *legsArmors();
    static const dataset_t *headArmors();
    static const dataset_t *charms();
    static const dataset_t *gsWeapons();
    static const dataset_t *snsWeapons();
    static const dataset_t *hWeapons();
    static const dataset_t *lWeapons();
    static const dataset_t *lbgWeapons();
    static const dataset_t *hbgWeapons();
    static const dataset_t *lsWeapons();
    static const dataset_t *saWeapons();
    static const dataset_t *glWeapons();
    static const dataset_t *bowWeapons();
    static const dataset_t *dbWeapons();
    static const dataset_t *hhWeapons();
    static const dataset_t *igWeapons();
    static const dataset_t *cbWeapons();
    static const lookup_dataset_t *lookups();
    static bool isRelicEquipment(std::uint8_t type, std::uint16_t identifier);
    static bool isRelicWeapon(std::uint8_t type, std::uint16_t identifier);

private:
    static dataset_t convert(const QVector<MH4GNamedValue> &source);
    static lookup_dataset_t convertLookups(const QVector<MH4GLookupValue> &source);
};

using MH3U_DS = MH4G_UI_DataAdapter;

class MH3U_Armory
{
public:
    static equipment_subtype_e convertSubtype(std::uint8_t equipmentType);
    static equipment_subtype_e convertSubtype(equipment_type_e equipmentType);
    static armor_t convertEquipmentToArmor(equipment_t &equipment);
    static void convertArmorToEquipment(armor_t &armor, equipment_t &equipment);
    static charm_t convertEquipmentToCharm(equipment_t &equipment);
    static void convertCharmToEquipment(charm_t &charm, equipment_t &equipment);
    static weapon_t convertEquipmentToWeapon(equipment_t &equipment);
    static void convertWeaponToEquipment(weapon_t &weapon, equipment_t &equipment);
};

#endif
