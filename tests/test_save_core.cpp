#include "mh4g.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <array>
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
