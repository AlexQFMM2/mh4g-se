#ifndef QARMOR_HPP
#define QARMOR_HPP

#include "main.hpp"

#include "qequipment.hpp"

#include <QWidget>
#include <QDialog>
#include <QSpinBox>
#include <QComboBox>

class QCheckBox;
class QGroupBox;
class QLabel;

class QArmor : public QEquipment
{
    Q_OBJECT
public:
    explicit QArmor(armor_t *armor, QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *);

private slots:
    void saveAndAccept();
    void updateAdvancedState();
    void updateCalculatedValues();
    void identifierChanged();
    void populateIdentifiers();

private:
    armor_t *armor;
    QComboBox *m_equipmentType;
    QSpinBox *m_upgradeLevel;
    QComboBox *m_identifier;
    QCheckBox *m_onlyRelicArmors;
    QComboBox *m_firstJewelIdentifier;
    QComboBox *m_secondJewelIdentifier;
    QComboBox *m_thirdJewelIdentifier;
    QCheckBox *m_firstJewelFixed;
    QCheckBox *m_secondJewelFixed;
    QCheckBox *m_thirdJewelFixed;
    QCheckBox *m_enableAdvanced;
    QGroupBox *m_advancedGroup;
    QComboBox *m_resistance;
    QComboBox *m_defense;
    QComboBox *m_relicUpgrade;
    QComboBox *m_slots;
    QComboBox *m_rarity;
    QComboBox *m_polishRequirement;
    QCheckBox *m_unpolished;
    QCheckBox *m_glow;
    QLabel *m_resistanceValue;
    QLabel *m_defenseTrueValue;
    QLabel *m_defensePanelValue;

    void load();
    void save();
    bool validate();
    void populateByteCombo(QComboBox *combo, const QString &domain, const QString &variant,
                           uint8_t currentValue);
    int byteComboValue(QComboBox *combo, bool *ok = NULL) const;
    QString comboLookupDisplay(QComboBox *combo) const;
};

#endif // QARMOR_HPP
