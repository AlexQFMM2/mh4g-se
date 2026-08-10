#ifndef MH4G_HPP
#define MH4G_HPP

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

#include <array>
#include <cstdint>

struct MH4GNamedValue
{
    int id = 0;
    QString name;
    QString english;
};

class MH4GData
{
public:
    bool load(const QString &language, QString *error = nullptr);
    QString language() const;
    const QVector<MH4GNamedValue> &items() const;
    const QVector<MH4GNamedValue> &skills() const;
    const QVector<MH4GNamedValue> &equipmentTypes() const;
    const QVector<MH4GNamedValue> &decorations() const;
    const QVector<MH4GNamedValue> &equipment(int type) const;
    QString itemName(int id) const;
    QString equipmentTypeName(int type) const;
    QString equipmentName(int type, int id) const;
    QString decorationName(int id) const;

private:
    bool loadCsv(const QString &fileName, QVector<MH4GNamedValue> &values, QString *error);
    QString locateDataRoot() const;
    static QString nameFor(const QVector<MH4GNamedValue> &values, int id);

    QString m_language;
    QVector<MH4GNamedValue> m_items;
    QVector<MH4GNamedValue> m_skills;
    QVector<MH4GNamedValue> m_types;
    QVector<MH4GNamedValue> m_decorations;
    QHash<int, QVector<MH4GNamedValue>> m_equipment;
};

class MH4GSave
{
public:
    static constexpr int FileSize = 81408;
    static constexpr int DataOffset = 8;
    static constexpr int ItemOffset = 0x015E;
    static constexpr int ItemCount = 1400;
    static constexpr int ItemSize = 4;
    static constexpr int EquipmentOffset = 0x173E;
    static constexpr int EquipmentCount = 1500;
    static constexpr int EquipmentSize = 28;

    struct Item
    {
        std::uint16_t id = 0;
        std::uint16_t count = 0;
    };

    struct Character
    {
        QString name;
        std::uint8_t sex = 0;
        std::uint8_t hair = 0;
        std::uint8_t underwear = 0;
        std::uint8_t voice = 0;
        std::uint32_t hunterRank = 0;
        std::uint32_t money = 0;
    };

    using Equipment = std::array<std::uint8_t, EquipmentSize>;

    bool load(const QString &fileName);
    bool save(const QString &fileName);
    bool loaded() const;
    QString fileName() const;
    QString lastError() const;

    Item item(int slot) const;
    void setItem(int slot, Item value);
    Character character() const;
    void setCharacter(const Character &value);
    Equipment equipment(int slot) const;
    void setEquipment(int slot, const Equipment &value);
    void patchEquipmentBasic(int slot, std::uint8_t type, std::uint8_t levelOrSlots,
                             std::uint16_t identifier,
                             const std::array<std::uint16_t, 3> &decorations);

    QByteArray decryptedBytes() const;
    static bool decrypt(const QByteArray &encrypted, QByteArray &decrypted, QString *error = nullptr);
    static bool encrypt(const QByteArray &decrypted, QByteArray &encrypted, QString *error = nullptr);

private:
    static std::uint16_t read16(const QByteArray &data, int offset);
    static std::uint32_t read32(const QByteArray &data, int offset);
    static void write16(QByteArray &data, int offset, std::uint16_t value);
    static void write32(QByteArray &data, int offset, std::uint32_t value);
    static void swapDwords(QByteArray &data);
    static void xorPayload(QByteArray &data, std::uint16_t seed);
    static bool cryptBlowfish(const QByteArray &input, QByteArray &output, bool encrypting, QString *error);

    QByteArray m_decrypted;
    QString m_fileName;
    QString m_error;
};

#endif
