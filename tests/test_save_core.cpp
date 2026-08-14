#include "mh4g.hpp"
#include "mh4g_equipment_values.hpp"
#include "mh4g_transfer.hpp"
#include "mh4g_ui_compat.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>

namespace
{
bool require(bool condition, const QString &message)
{
    if (!condition) std::cerr << message.toStdString() << std::endl;
    return condition;
}

bool testSave(const QString &path, const QString &knownDecrypted = QString())
{
    QFile source(path);
    if (!require(source.open(QIODevice::ReadOnly), "cannot read sample: " + path)) return false;
    const QByteArray originalEncrypted = source.readAll();

    MH4GSave save;
    if (!require(save.load(path), "load failed: " + save.lastError())) return false;
    const QByteArray originalDecrypted = save.decryptedBytes();

    // The GUI adapter writes supported character fields back even when the
    // user only opens and saves a file. Reapplying them must be lossless.
    save.setCharacter(save.character());
    if (!require(save.decryptedBytes() == originalDecrypted,
                 "reapplying character data changed the decrypted save")) return false;

    if (!knownDecrypted.isEmpty())
    {
        QFile known(knownDecrypted);
        if (!require(known.open(QIODevice::ReadOnly), "cannot read known decrypted file")) return false;
        if (!require(known.readAll() == originalDecrypted, "decryption differs from known-good output")) return false;
    }

    QTemporaryDir directory;
    if (!require(directory.isValid(), "cannot create temporary directory")) return false;
    const QString unchangedPath = directory.filePath("unchanged-user");
    if (!require(save.save(unchangedPath), "unchanged save failed: " + save.lastError())) return false;
    QFile unchanged(unchangedPath);
    if (!require(unchanged.open(QIODevice::ReadOnly), "cannot read unchanged result")) return false;
    if (!require(unchanged.readAll() == originalEncrypted, "unchanged encrypted round trip is not byte-identical")) return false;

    MH4G_UI_SaveAdapter adapter;
    if (!require(adapter.load(path.toStdString()), "GUI adapter load failed")) return false;
    const QString adapterPath = directory.filePath("adapter-unchanged-user");
    if (!require(adapter.save(adapterPath.toStdString()), "GUI adapter save failed")) return false;
    QFile adapterOutput(adapterPath);
    if (!require(adapterOutput.open(QIODevice::ReadOnly), "cannot read GUI adapter result") ||
        !require(adapterOutput.readAll() == originalEncrypted,
                 "unchanged GUI adapter round trip is not byte-identical")) return false;

    const int itemSlot = MH4GSave::ItemCount - 1;
    const MH4GSave::Item oldItem = save.item(itemSlot);
    save.setItem(itemSlot, {static_cast<std::uint16_t>(oldItem.id ^ 0x55aa),
                            static_cast<std::uint16_t>(oldItem.count ^ 0xaa55)});

    const int equipmentSlot = MH4GSave::EquipmentCount - 1;
    const MH4GSave::Equipment oldEquipment = save.equipment(equipmentSlot);
    const std::array<std::uint16_t, 3> decorations = {0x810f, 0x0022, 0xffff};
    save.patchEquipmentBasic(equipmentSlot, 19, 99, 157, decorations);
    const MH4GSave::Equipment newEquipment = save.equipment(equipmentSlot);
    const std::array<bool, MH4GSave::EquipmentSize> editable = {
        true, true, true, true, false, false,
        true, true, true, true, true, true,
        false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false,
    };
    for (int index = 0; index < MH4GSave::EquipmentSize; ++index)
    {
        if (!editable[index] && !require(newEquipment[index] == oldEquipment[index],
                                         QString("basic equipment patch changed byte 0x%1").arg(index, 2, 16, QChar('0'))))
            return false;
    }

    bool synchronizedEquippedCopy = false;
    const QByteArray beforeEquippedEdit = save.decryptedBytes();
    for (int equippedSlot = 0; equippedSlot < MH4GSave::EquippedEquipmentCount; ++equippedSlot)
    {
        const int indexOffset = MH4GSave::DataOffset + MH4GSave::EquippedEquipmentIndexOffset + equippedSlot * 2;
        const int boxSlot = static_cast<unsigned char>(beforeEquippedEdit.at(indexOffset)) |
            (static_cast<int>(static_cast<unsigned char>(beforeEquippedEdit.at(indexOffset + 1))) << 8);
        if (boxSlot < 0 || boxSlot >= MH4GSave::EquipmentCount) continue;
        MH4GSave::Equipment equipped = save.equipment(boxSlot);
        equipped[27] ^= 0x5a;
        save.setEquipment(boxSlot, equipped);
        const QByteArray afterEquippedEdit = save.decryptedBytes();
        const int copyOffset = MH4GSave::DataOffset + MH4GSave::EquippedEquipmentOffset +
                               equippedSlot * MH4GSave::EquipmentSize;
        if (!require(std::memcmp(afterEquippedEdit.constData() + copyOffset, equipped.data(),
                                 MH4GSave::EquipmentSize) == 0,
                     "editing an equipped box slot did not synchronize its current-equipment copy"))
            return false;
        synchronizedEquippedCopy = true;
        break;
    }
    if (!require(synchronizedEquippedCopy, "sample has no valid equipped equipment index")) return false;

    const QString editedPath = directory.filePath("edited-user");
    if (!require(save.save(editedPath), "edited save failed: " + save.lastError())) return false;
    MH4GSave reloaded;
    if (!require(reloaded.load(editedPath), "edited reload failed: " + reloaded.lastError())) return false;
    if (!require(reloaded.decryptedBytes().mid(MH4GSave::DataOffset) ==
                 save.decryptedBytes().mid(MH4GSave::DataOffset),
                 "edited data changed during encrypt/decrypt round trip")) return false;
    if (!require(reloaded.item(itemSlot).id == static_cast<std::uint16_t>(oldItem.id ^ 0x55aa),
                 "edited item ID did not persist")) return false;
    if (!require(reloaded.equipment(equipmentSlot) == save.equipment(equipmentSlot),
                 "edited equipment did not persist")) return false;
    return true;
}

bool testTransferForms()
{
    save_t source{};
    source.chest[13][99] = {1913, 99};
    source.box[14][99][0] = 20;
    source.box[14][99][1] = 7;
    source.box[14][99][2] = 0x34;
    source.box[14][99][3] = 0x12;
    source.box[14][99][27] = 0xa5;

    std::vector<MH3U_Transfer::chest_entry_t> items;
    std::vector<MH3U_Transfer::equipment_entry_t> equipment;
    std::string error;
    if (!require(MH3U_Transfer::parseChest(MH3U_Transfer::exportChest(source), items, error),
                 QString::fromStdString(error)) ||
        !require(items.size() == 1400, "item form did not contain 1400 slots")) return false;
    if (!require(MH3U_Transfer::parseEquipmentBox(MH3U_Transfer::exportEquipmentBox(source), equipment, error),
                 QString::fromStdString(error)) ||
        !require(equipment.size() == 1500, "equipment form did not contain 1500 slots")) return false;

    save_t target{};
    MH3U_Transfer::applyChest(items, target);
    MH3U_Transfer::applyEquipmentBox(equipment, target);
    return require(target.chest[13][99].id == 1913 && target.chest[13][99].count == 99,
                   "last item form slot did not round trip") &&
        require(std::memcmp(target.box[14][99], source.box[14][99], EQUIPMENT_SIZE) == 0,
                "last equipment form slot did not round trip");
}

bool testEquipmentValues()
{
    const std::array<double, 14> multipliers = {
        4.8, 1.4, 5.2, 2.3, 1.3, 1.5, 3.3,
        5.4, 2.3, 1.2, 1.4, 5.2, 3.1, 3.6,
    };
    for (int index = 0; index < static_cast<int>(multipliers.size()); ++index)
    {
        const int type = MH4G_Type::GSType + index;
        if (!require(MH4GEquipmentValues::isWeaponType(type),
                     QString("equipment type %1 was not recognized as a weapon").arg(type)) ||
            !require(std::abs(MH4GEquipmentValues::attackDisplayMultiplier(type) - multipliers[index]) < 0.0001,
                     QString("wrong attack multiplier for equipment type %1").arg(type)))
            return false;
    }

    const MH4GAttackResult specialGreatSword =
        MH4GEquipmentValues::attack(MH4G_Type::GSType, 0x5b, 0);
    if (!require(specialGreatSword.known && specialGreatSword.trueRaw == 355 &&
                 specialGreatSword.panelValue == 1704,
                 "great sword special tier 0x5B did not calculate as 355 / 1704"))
        return false;

    const MH4GAttackResult boostedGreatSword =
        MH4GEquipmentValues::attack(MH4G_Type::GSType, 0x14, 2);
    if (!require(boostedGreatSword.known && boostedGreatSword.baseTrueRaw == 340 &&
                 boostedGreatSword.weaponBonus == 20 && boostedGreatSword.trueRaw == 360 &&
                 boostedGreatSword.panelValue == 1728,
                 "great sword 340 + RAW+20 did not calculate as 360 / 1728"))
        return false;

    const MH4GAttackResult upgradedAndHonedGreatSword =
        MH4GEquipmentValues::attack(MH4G_Type::GSType, 0x14, 0, 3, 0x40);
    if (!require(upgradedAndHonedGreatSword.known &&
                 upgradedAndHonedGreatSword.modifiersKnown &&
                 upgradedAndHonedGreatSword.upgradeBonusEstimated &&
                 upgradedAndHonedGreatSword.upgradeBonus == 30 &&
                 upgradedAndHonedGreatSword.honingBonus == 20 &&
                 upgradedAndHonedGreatSword.trueRaw == 390 &&
                 upgradedAndHonedGreatSword.panelValue == 1872,
                 "great sword relic upgrade reference and Attack Honing were not included"))
        return false;

    const MH4GAttackResult unknownModifiers =
        MH4GEquipmentValues::attack(MH4G_Type::GSType, 0x14, 0, 0xff, 0xff);
    if (!require(unknownModifiers.known && !unknownModifiers.modifiersKnown &&
                 unknownModifiers.honingMode == 0xC0 &&
                 unknownModifiers.honingExtraBits == 0x3F &&
                 unknownModifiers.honingBonus == 0 && unknownModifiers.trueRaw == 340,
                 "unknown relic modifiers should retain the known attack subtotal"))
        return false;

    const MH4GAttackResult customAttackHoning =
        MH4GEquipmentValues::attack(MH4G_Type::GSType, 0x14, 0, 0, 0x7f);
    if (!require(customAttackHoning.modifiersKnown &&
                 customAttackHoning.honingMode == 0x40 &&
                 customAttackHoning.honingExtraBits == 0x3f &&
                 customAttackHoning.honingBonus == 20 &&
                 customAttackHoning.trueRaw == 360,
                 "custom honing byte did not decode its upper category bits"))
        return false;

    const MH4GLookupNumber awakened = MH4GEquipmentValues::parseLookupNumber("(1000)");
    const MH4GLookupNumber normal = MH4GEquipmentValues::parseLookupNumber("750");
    const MH4GSharpnessResult purple = MH4GEquipmentValues::sharpness(0x15);
    const MH4GSharpnessResult overflow = MH4GEquipmentValues::sharpness(0xda);
    const MH4GSharpnessResult unknownSharpness = MH4GEquipmentValues::sharpness(0xff);
    for (int code = 0; code <= 0x15; ++code)
    {
        const MH4GSharpnessResult sharpness = MH4GEquipmentValues::sharpness(static_cast<std::uint8_t>(code));
        if (!require(sharpness.known && !sharpness.provisional &&
                     sharpness.total() > 0 && sharpness.total() <= 450,
                     QString("normal sharpness 0x%1 is missing or invalid").arg(code, 2, 16, QChar('0'))))
            return false;
        for (int length : sharpness.lengths)
        {
            if (!require(length >= 0 && length % 5 == 0,
                         QString("sharpness 0x%1 is not expressed in five-point units")
                             .arg(code, 2, 16, QChar('0'))))
                return false;
        }
    }
    return require(awakened.known && awakened.awakened && awakened.value == 1000,
                   "awakened attribute value was not parsed") &&
        require(normal.known && !normal.awakened && normal.value == 750,
                   "normal attribute value was not parsed") &&
        require(purple.known && !purple.provisional && purple.total() == 450 &&
                purple.lengths[0] == 95 && purple.lengths[6] == 25,
                "sharpness 0x15 did not decode to the reconstructed purple scheme") &&
        require(overflow.known && overflow.provisional && overflow.total() == 450 &&
                overflow.lengths[0] == 235 && overflow.lengths[6] == 215,
                "sharpness 0xDA did not decode to the provisional red/purple scheme") &&
        require(!unknownSharpness.known,
                "unknown sharpness code should remain unknown") &&
        require(MH4GEquipmentValues::usesMeleeSharpness(MH4G_Type::GSType),
                "great sword should use melee sharpness") &&
        require(!MH4GEquipmentValues::usesMeleeSharpness(MH4G_Type::LBGType),
                "light bowgun should not use melee sharpness");
}

bool testEquipmentConversionPreservesBytes()
{
    equipment_t original{};
    for (int index = 0; index < EQUIPMENT_SIZE; ++index)
        original[index] = static_cast<std::uint8_t>(index * 7 + 3);
    original[0] = MH4G_Type::GSType;

    equipment_t weaponOutput{};
    weapon_t weapon = MH3U_Armory::convertEquipmentToWeapon(original);
    weapon.raw[12] = 0xe7;
    MH3U_Armory::convertWeaponToEquipment(weapon, weaponOutput);
    for (int index = 0; index < EQUIPMENT_SIZE; ++index)
    {
        const std::uint8_t expected = index == 12 ? 0xe7 : original[index];
        if (!require(weaponOutput[index] == expected,
                     QString("weapon one-byte patch changed byte 0x%1").arg(index, 2, 16, QChar('0'))))
            return false;
    }

    original[0] = MH4G_Type::ChestType;
    equipment_t armorOutput{};
    armor_t armor = MH3U_Armory::convertEquipmentToArmor(original);
    armor.raw[13] = 0xd4;
    MH3U_Armory::convertArmorToEquipment(armor, armorOutput);
    for (int index = 0; index < EQUIPMENT_SIZE; ++index)
    {
        const std::uint8_t expected = index == 13 ? 0xd4 : original[index];
        if (!require(armorOutput[index] == expected,
                     QString("armor one-byte patch changed byte 0x%1").arg(index, 2, 16, QChar('0'))))
            return false;
    }
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    MH4GData data;
    QString dataError;
    if (!require(data.load("cn", &dataError), "Chinese data load failed: " + dataError) ||
        !require(data.items().size() == 1937, "unexpected Chinese item count") ||
        !require(data.equipmentTypes().size() == 21, "unexpected equipment type count") ||
        !require(data.decorations().size() == 290, "unexpected decoration count") ||
        !require(!data.lookups().isEmpty(), "equipment lookups were not loaded") ||
        !require(!data.itemName(1).isEmpty(), "Chinese item lookup failed") ||
        !require(!data.equipmentName(19, 157).isEmpty(), "Chinese equipment lookup failed") ||
        !require(data.equipmentName(MH4G_Type::WaistType, 101) == QStringLiteral("青熊腰甲"),
                 "waist game ID 101 should be 青熊腰甲") ||
        !require(data.equipmentName(MH4G_Type::WaistType, 112) == QStringLiteral("轰龙裙甲Ｓ"),
                 "waist game ID 112 should be 轰龙裙甲Ｓ") ||
        !require(data.isRelicEquipment(MH4G_Type::GSType, 239),
                 "great sword 239 should be marked as relic") ||
        !require(!data.isRelicEquipment(MH4G_Type::GSType, 154),
                 "great sword 154 should not be marked as relic") ||
        !require(!data.isRelicEquipment(MH4G_Type::LBGType, 29),
                 "light bowgun 29 should not be marked as relic"))
        return 1;
    if (!require(data.load("en", &dataError), "English data load failed: " + dataError) ||
        !require(data.items().size() == 1937, "unexpected English item count") ||
        !require(data.equipmentName(MH4G_Type::WaistType, 101) == QStringLiteral("Arzuros Faulds"),
                 "English waist game ID 101 should be Arzuros Faulds") ||
        !require(!data.itemName(1).isEmpty(), "English item lookup failed"))
        return 1;
    if (!testTransferForms() || !testEquipmentValues() ||
        !testEquipmentConversionPreservesBytes()) return 1;

    if (argc < 2)
    {
        std::cerr << "usage: test_save_core ENCRYPTED_SAVE [KNOWN_DECRYPTED] ..." << std::endl;
        return 2;
    }
    for (int index = 1; index < argc; ++index)
    {
        QString known;
        const QString argument = QString::fromLocal8Bit(argv[index]);
        const int separator = argument.indexOf('=');
        const QString encrypted = separator < 0 ? argument : argument.left(separator);
        if (separator >= 0) known = argument.mid(separator + 1);
        if (!testSave(encrypted, known)) return 1;
        std::cout << "ok: " << encrypted.toStdString() << std::endl;
    }
    return 0;
}
