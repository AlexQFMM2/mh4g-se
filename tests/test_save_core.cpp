#include "mh4g.hpp"
#include "mh4g_transfer.hpp"
#include "mh4g_ui_compat.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <array>
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

    const QString editedPath = directory.filePath("edited-user");
    if (!require(save.save(editedPath), "edited save failed: " + save.lastError())) return false;
    MH4GSave reloaded;
    if (!require(reloaded.load(editedPath), "edited reload failed: " + reloaded.lastError())) return false;
    if (!require(reloaded.decryptedBytes().mid(MH4GSave::DataOffset) ==
                 save.decryptedBytes().mid(MH4GSave::DataOffset),
                 "edited data changed during encrypt/decrypt round trip")) return false;
    if (!require(reloaded.item(itemSlot).id == static_cast<std::uint16_t>(oldItem.id ^ 0x55aa),
                 "edited item ID did not persist")) return false;
    if (!require(reloaded.equipment(equipmentSlot) == newEquipment,
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
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    MH4GData data;
    QString dataError;
    if (!require(data.load("cn", &dataError), "Chinese data load failed: " + dataError) ||
        !require(data.items().size() == 1913, "unexpected Chinese item count") ||
        !require(data.equipmentTypes().size() == 21, "unexpected equipment type count") ||
        !require(data.decorations().size() == 290, "unexpected decoration count") ||
        !require(!data.itemName(1).isEmpty(), "Chinese item lookup failed") ||
        !require(!data.equipmentName(19, 157).isEmpty(), "Chinese equipment lookup failed"))
        return 1;
    if (!require(data.load("en", &dataError), "English data load failed: " + dataError) ||
        !require(data.items().size() == 1913, "unexpected English item count") ||
        !require(!data.itemName(1).isEmpty(), "English item lookup failed"))
        return 1;
    if (!testTransferForms()) return 1;

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
