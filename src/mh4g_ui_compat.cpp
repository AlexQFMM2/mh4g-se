#include "mh4g_ui_compat.hpp"

#include <QString>

#include <cstring>
#include <map>

namespace
{
MH4GData g_data;
lang_t g_language = LANG_NONE;
dataset_t g_items;
dataset_t g_skills;
dataset_t g_jewels;
dataset_t g_types;
std::map<int, dataset_t> g_equipment;
lookup_dataset_t g_lookups;

std::uint16_t read16(const equipment_t &record, int offset)
{
    return record[offset] | (static_cast<std::uint16_t>(record[offset + 1]) << 8);
}

std::int16_t readSigned16(const equipment_t &record, int offset)
{
    return static_cast<std::int16_t>(read16(record, offset));
}

void write16(equipment_t &record, int offset, std::uint16_t value)
{
    record[offset] = static_cast<std::uint8_t>(value & 0xff);
    record[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}
}

MH4G_UI_SaveAdapter::MH4G_UI_SaveAdapter() : savedata(new save_t())
{
    std::memset(savedata, 0, sizeof(save_t));
}

MH4G_UI_SaveAdapter::~MH4G_UI_SaveAdapter()
{
    delete savedata;
    savedata = nullptr;
}

bool MH4G_UI_SaveAdapter::loaded() const { return m_save.loaded(); }

bool MH4G_UI_SaveAdapter::load(const std::string &input)
{
    if (!m_save.load(QString::fromStdString(input))) return false;
    importModel();
    return true;
}

bool MH4G_UI_SaveAdapter::save(const std::string &output)
{
    if (!loaded()) return false;
    exportModel();
    return m_save.save(QString::fromStdString(output));
}

bool MH4G_UI_SaveAdapter::save()
{
    if (!loaded()) return false;
    return save(m_save.fileName().toStdString());
}

std::string MH4G_UI_SaveAdapter::lastError() const { return m_save.lastError().toStdString(); }
std::string MH4G_UI_SaveAdapter::currentFilename() const { return m_save.fileName().toStdString(); }
std::string MH4G_UI_SaveAdapter::formatName() const { return "MH4G 3DS"; }
std::uint32_t MH4G_UI_SaveAdapter::nameSize() const { return 11; }

void MH4G_UI_SaveAdapter::importModel()
{
    const MH4GSave::Character character = m_save.character();
    savedata->sex = character.sex;
    savedata->face = character.underwear;
    savedata->hair = character.hair;
    std::memset(savedata->name, 0, sizeof(savedata->name));
    for (int index = 0; index < character.name.size() && index < 11; ++index)
        savedata->name[index] = character.name.at(index).unicode();
    savedata->money = character.money;
    savedata->voice = character.voice;
    savedata->mogapoint = character.hunterRank;
    for (int index = 0; index < MH4GSave::ItemCount; ++index)
    {
        const MH4GSave::Item value = m_save.item(index);
        savedata->chest[index / 100][index % 100] = {value.id, value.count};
    }
    for (int index = 0; index < MH4GSave::EquipmentCount; ++index)
    {
        const MH4GSave::Equipment value = m_save.equipment(index);
        std::memcpy(savedata->box[index / 100][index % 100], value.data(), EQUIPMENT_SIZE);
    }
}

void MH4G_UI_SaveAdapter::exportModel()
{
    MH4GSave::Character character;
    QString name;
    for (int index = 0; index < 12 && savedata->name[index] != 0; ++index)
        name.append(QChar(savedata->name[index]));
    character.name = name;
    character.sex = savedata->sex;
    character.underwear = savedata->face;
    character.hair = savedata->hair;
    character.voice = savedata->voice;
    character.money = savedata->money;
    character.hunterRank = savedata->mogapoint;
    m_save.setCharacter(character);
    for (int index = 0; index < MH4GSave::ItemCount; ++index)
    {
        const item_t &value = savedata->chest[index / 100][index % 100];
        m_save.setItem(index, {value.id, value.count});
    }
    for (int index = 0; index < MH4GSave::EquipmentCount; ++index)
    {
        MH4GSave::Equipment value{};
        std::memcpy(value.data(), savedata->box[index / 100][index % 100], EQUIPMENT_SIZE);
        m_save.setEquipment(index, value);
    }
}

dataset_t MH4G_UI_DataAdapter::convert(const QVector<MH4GNamedValue> &source)
{
    dataset_t result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const MH4GNamedValue &value : source)
    {
        dataitem_t item;
        item.count = static_cast<std::uint32_t>(value.id);
        item.name = value.name.toStdString();
        item.english = value.english.toStdString();
        item.identifier = item.name.empty() ? item.english : item.name;
        item.source = "mh4g-data";
        if (!value.source.isEmpty()) item.source = value.source.toStdString();
        item.rarity = value.rarity;
        item.isRelic = value.isRelic;
        result.push_back(item);
    }
    return result;
}

lookup_dataset_t MH4G_UI_DataAdapter::convertLookups(const QVector<MH4GLookupValue> &source)
{
    lookup_dataset_t result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const MH4GLookupValue &value : source)
    {
        lookupitem_t item;
        item.domain = value.domain.toStdString();
        item.equipmentType = static_cast<std::uint8_t>(value.equipmentType);
        item.variant = value.variant.toStdString();
        item.value = static_cast<std::uint8_t>(value.value);
        item.identifier = value.name.toStdString();
        item.english = value.english.toStdString();
        item.source = value.source.toStdString();
        result.push_back(item);
    }
    return result;
}

bool MH4G_UI_DataAdapter::readData(lang_t lang)
{
    if (lang == LANG_NONE) return false;
    QString error;
    if (!g_data.load(lang == LANG_EN ? "en" : "cn", &error)) return false;
    g_language = lang;
    g_items = convert(g_data.items());
    g_skills = convert(g_data.skills());
    g_jewels = convert(g_data.decorations());
    g_types = convert(g_data.equipmentTypes());
    g_lookups = convertLookups(g_data.lookups());
    g_equipment.clear();
    for (int type = 1; type <= 20; ++type)
        g_equipment.emplace(type, convert(g_data.equipment(type)));
    return true;
}

bool MH4G_UI_DataAdapter::deleteData()
{
    g_items.clear(); g_skills.clear(); g_jewels.clear(); g_types.clear(); g_equipment.clear(); g_lookups.clear();
    g_language = LANG_NONE;
    return true;
}

lang_t MH4G_UI_DataAdapter::lang() { return g_language; }
const dataset_t *MH4G_UI_DataAdapter::items() { return &g_items; }
const dataset_t *MH4G_UI_DataAdapter::skills() { return &g_skills; }
const dataset_t *MH4G_UI_DataAdapter::jewels() { return &g_jewels; }
const dataset_t *MH4G_UI_DataAdapter::equipmentTypes() { return &g_types; }
const dataset_t *MH4G_UI_DataAdapter::equipment(std::uint8_t type)
{
    auto it = g_equipment.find(type);
    return it == g_equipment.end() ? nullptr : &it->second;
}
const dataset_t *MH4G_UI_DataAdapter::chestArmors() { return equipment(1); }
const dataset_t *MH4G_UI_DataAdapter::armsArmors() { return equipment(2); }
const dataset_t *MH4G_UI_DataAdapter::waistArmors() { return equipment(3); }
const dataset_t *MH4G_UI_DataAdapter::legsArmors() { return equipment(4); }
const dataset_t *MH4G_UI_DataAdapter::headArmors() { return equipment(5); }
const dataset_t *MH4G_UI_DataAdapter::charms() { return equipment(6); }
const dataset_t *MH4G_UI_DataAdapter::gsWeapons() { return equipment(7); }
const dataset_t *MH4G_UI_DataAdapter::snsWeapons() { return equipment(8); }
const dataset_t *MH4G_UI_DataAdapter::hWeapons() { return equipment(9); }
const dataset_t *MH4G_UI_DataAdapter::lWeapons() { return equipment(10); }
const dataset_t *MH4G_UI_DataAdapter::lbgWeapons() { return equipment(11); }
const dataset_t *MH4G_UI_DataAdapter::hbgWeapons() { return equipment(12); }
const dataset_t *MH4G_UI_DataAdapter::lsWeapons() { return equipment(13); }
const dataset_t *MH4G_UI_DataAdapter::saWeapons() { return equipment(14); }
const dataset_t *MH4G_UI_DataAdapter::glWeapons() { return equipment(15); }
const dataset_t *MH4G_UI_DataAdapter::bowWeapons() { return equipment(16); }
const dataset_t *MH4G_UI_DataAdapter::dbWeapons() { return equipment(17); }
const dataset_t *MH4G_UI_DataAdapter::hhWeapons() { return equipment(18); }
const dataset_t *MH4G_UI_DataAdapter::igWeapons() { return equipment(19); }
const dataset_t *MH4G_UI_DataAdapter::cbWeapons() { return equipment(20); }
const lookup_dataset_t *MH4G_UI_DataAdapter::lookups() { return &g_lookups; }
bool MH4G_UI_DataAdapter::isRelicEquipment(std::uint8_t type, std::uint16_t identifier)
{
    if (identifier == 0) return false;
    const dataset_t *values = equipment(type);
    if (values == nullptr) return false;
    for (const dataitem_t &value : *values)
        if (value.count == identifier) return value.isRelic;
    return false;
}
bool MH4G_UI_DataAdapter::isRelicWeapon(std::uint8_t type, std::uint16_t identifier)
{
    return type >= MH4G_Type::GSType && type <= MH4G_Type::CBType &&
           isRelicEquipment(type, identifier);
}

equipment_subtype_e MH3U_Armory::convertSubtype(std::uint8_t type)
{
    return convertSubtype(static_cast<equipment_type_e>(type));
}

equipment_subtype_e MH3U_Armory::convertSubtype(equipment_type_e type)
{
    if (type >= MH4G_Type::ChestType && type <= MH4G_Type::HeadType) return MH4G_Type::ArmorSubtype;
    if (type == MH4G_Type::CharmType) return MH4G_Type::CharmSubtype;
    if (type >= MH4G_Type::GSType && type <= MH4G_Type::CBType) return MH4G_Type::WeaponSubtype;
    return MH4G_Type::NoneSubtype;
}

armor_t MH3U_Armory::convertEquipmentToArmor(equipment_t &equipment)
{
    armor_t result{};
    std::memcpy(result.raw, equipment, EQUIPMENT_SIZE);
    result.equipmentType = equipment[0];
    result.upgradeLevel = equipment[1];
    result.identifier = read16(equipment, 2);
    result.firstJewelIdentifier = read16(equipment, 6);
    result.secondJewelIdentifier = read16(equipment, 8);
    result.thirdJewelIdentifier = read16(equipment, 10);
    return result;
}

void MH3U_Armory::convertArmorToEquipment(armor_t &armor, equipment_t &equipment)
{
    std::memcpy(equipment, armor.raw, EQUIPMENT_SIZE);
    equipment[0] = armor.equipmentType;
    equipment[1] = armor.upgradeLevel;
    write16(equipment, 2, armor.identifier);
    write16(equipment, 6, armor.firstJewelIdentifier);
    write16(equipment, 8, armor.secondJewelIdentifier);
    write16(equipment, 10, armor.thirdJewelIdentifier);
}

weapon_t MH3U_Armory::convertEquipmentToWeapon(equipment_t &equipment)
{
    weapon_t result{};
    std::memcpy(result.raw, equipment, EQUIPMENT_SIZE);
    result.equipmentType = equipment[0];
    result.levelOrModification = equipment[1];
    result.identifier = read16(equipment, 2);
    result.firstJewelIdentifier = read16(equipment, 6);
    result.secondJewelIdentifier = read16(equipment, 8);
    result.thirdJewelIdentifier = read16(equipment, 10);
    return result;
}

void MH3U_Armory::convertWeaponToEquipment(weapon_t &weapon, equipment_t &equipment)
{
    std::memcpy(equipment, weapon.raw, EQUIPMENT_SIZE);
    equipment[0] = weapon.equipmentType;
    equipment[1] = weapon.levelOrModification;
    write16(equipment, 2, weapon.identifier);
    write16(equipment, 6, weapon.firstJewelIdentifier);
    write16(equipment, 8, weapon.secondJewelIdentifier);
    write16(equipment, 10, weapon.thirdJewelIdentifier);
}

charm_t MH3U_Armory::convertEquipmentToCharm(equipment_t &equipment)
{
    charm_t result{};
    std::memcpy(result.raw, equipment, EQUIPMENT_SIZE);
    result.equipmentType = equipment[0];
    result.slotsCount = equipment[1];
    result.identifier = read16(equipment, 2);
    result.firstJewelIdentifier = read16(equipment, 6);
    result.secondJewelIdentifier = read16(equipment, 8);
    result.thirdJewelIdentifier = read16(equipment, 10);
    result.firstSkillIdentifier = read16(equipment, 12);
    result.firstSkillValue = readSigned16(equipment, 14);
    result.secondSkillIdentifier = read16(equipment, 16);
    result.secondSkillValue = readSigned16(equipment, 18);
    return result;
}

void MH3U_Armory::convertCharmToEquipment(charm_t &charm, equipment_t &equipment)
{
    std::memcpy(equipment, charm.raw, EQUIPMENT_SIZE);
    equipment[0] = charm.equipmentType;
    equipment[1] = charm.slotsCount;
    write16(equipment, 2, charm.identifier);
    write16(equipment, 6, charm.firstJewelIdentifier);
    write16(equipment, 8, charm.secondJewelIdentifier);
    write16(equipment, 10, charm.thirdJewelIdentifier);
    write16(equipment, 12, charm.firstSkillIdentifier);
    write16(equipment, 14, static_cast<std::uint16_t>(charm.firstSkillValue));
    write16(equipment, 16, charm.secondSkillIdentifier);
    write16(equipment, 18, static_cast<std::uint16_t>(charm.secondSkillValue));
}
