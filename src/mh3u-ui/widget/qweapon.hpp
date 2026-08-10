#ifndef QWEAPON_H
#define QWEAPON_H

#include "main.hpp"

#include "qequipment.hpp"

#include <QWidget>
#include <QDialog>

class QCheckBox;
class QGroupBox;
class QLabel;
class QSpinBox;

class QWeapon : public QEquipment
{
    Q_OBJECT
public:
    explicit QWeapon(weapon_t *weapon, QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *);

private slots:
    void saveAndAccept();
    void populateIdentifiers();
    void updateRelicState();
    void updateAttributeValueChoices();
    void updateCalculatedValues();
    void levelOrModificationChanged(int value);
    void bowgunFlagsChanged();

private:
    weapon_t *weapon;
    QComboBox *m_equipmentType;
    QComboBox *m_identifier;
    QCheckBox *m_onlyRelicWeapons;
    QComboBox *m_firstJewelIdentifier;
    QComboBox *m_secondJewelIdentifier;
    QComboBox *m_thirdJewelIdentifier;
    QCheckBox *m_firstJewelFixed;
    QCheckBox *m_secondJewelFixed;
    QCheckBox *m_thirdJewelFixed;
    QLabel *m_relicStatus;
    QGroupBox *m_advancedGroup;
    QComboBox *m_attributeType;
    QComboBox *m_attributeValue;
    QComboBox *m_attackTier;
    QComboBox *m_sharpness;
    QComboBox *m_upgrade;
    QComboBox *m_weaponSpecial;
    QComboBox *m_slots;
    QComboBox *m_rarity;
    QComboBox *m_polishRequirement;
    QComboBox *m_honing;
    QLabel *m_levelOrModificationLabel;
    QSpinBox *m_levelOrModification;
    QCheckBox *m_bowgunLimitBreakBit;
    QCheckBox *m_bowgunAttachmentBit;
    QLabel *m_bowgunRawSummary;
    QGroupBox *m_kinsectInstanceGroup;
    QSpinBox *m_kinsectValues[8];
    QCheckBox *m_unpolished;
    QCheckBox *m_glow;
    QLabel *m_attackTrueValue;
    QLabel *m_attackPanelValue;
    QLabel *m_attributeTrueValue;
    QLabel *m_attributePanelValue;
    QLabel *m_sharpnessValue;
    QLabel *m_calculationNote;

    void load();
    void save();
    bool validate();
    const dataset_t *weaponDataset() const;
    QString lookupVariant(const QString &domain) const;
    void populateByteCombo(QComboBox *combo, const QString &domain, const QString &variant,
                           uint8_t currentValue);
    int byteComboValue(QComboBox *combo, bool *ok = NULL) const;
    int identifierValue(bool *ok = NULL) const;
    QString comboLookupDisplay(QComboBox *combo) const;
    QString localizedLookupName(const QString &domain, uint8_t value, const QString &name) const;
    bool selectedWeaponIsRelic() const;
};

#endif // QWEAPON_H
