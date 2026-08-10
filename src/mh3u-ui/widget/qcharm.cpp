#include "qcharm.hpp"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

namespace
{
QString charmHexWord(uint16_t value)
{
    return QString("0x%1").arg(value, 4, 16, QChar('0')).toUpper();
}
}

QCharm::QCharm(charm_t *charm, QWidget *parent) : QEquipment(NULL, parent)
{
    this->charm = charm;

    m_equipmentType = new QComboBox(this);
    m_equipmentType->addItem(uiText("(None)"), 0);
    for (uint32_t i = 0; i < MH3U_DS::equipmentTypes()->size(); i++)
    {
        m_equipmentType->addItem(QString(MH3U_DS::equipmentTypes()->at(i).identifier.c_str()), MH3U_DS::equipmentTypes()->at(i).count);
    }
    configureSearchableComboBox(m_equipmentType);
    m_equipmentType->setEnabled(false);

    m_slotsCount = new QSpinBox(this);
    m_slotsCount->setMinimum(0x00);
    m_slotsCount->setMaximum(0xff);

    m_identifier = new QComboBox(this);
    m_identifier->addItem(uiText("(None)"), 0);
    populateEquipmentIdentifierComboBox(m_identifier, MH3U_DS::charms(), charm->identifier);
    configureSearchableComboBox(m_identifier);

    m_firstSkillIdentifier = new QComboBox(this);
    m_secondSkillIdentifier = new QComboBox(this);
    m_firstSkillIdentifier->addItem(uiText("(None)"), 0);
    m_secondSkillIdentifier->addItem(uiText("(None)"), 0);
    for (uint32_t i = 0; i < MH3U_DS::skills()->size(); i++)
    {
        m_firstSkillIdentifier->addItem(QString(MH3U_DS::skills()->at(i).identifier.c_str()), MH3U_DS::skills()->at(i).count);
        m_secondSkillIdentifier->addItem(QString(MH3U_DS::skills()->at(i).identifier.c_str()), MH3U_DS::skills()->at(i).count);
    }
    configureSearchableComboBox(m_firstSkillIdentifier);
    configureSearchableComboBox(m_secondSkillIdentifier);
    m_firstSkillValue = new QSpinBox(this);
    m_firstSkillValue->setMinimum(-32768);
    m_firstSkillValue->setMaximum(32767);
    m_secondSkillValue = new QSpinBox(this);
    m_secondSkillValue->setMinimum(-32768);
    m_secondSkillValue->setMaximum(32767);

    m_firstJewelIdentifier = new QComboBox(this);
    m_secondJewelIdentifier = new QComboBox(this);
    m_thirdJewelIdentifier = new QComboBox(this);
    m_firstJewelIdentifier->addItem(uiText("(None)"), 0);
    m_secondJewelIdentifier->addItem(uiText("(None)"), 0);
    m_thirdJewelIdentifier->addItem(uiText("(None)"), 0);
    for (uint32_t i = 0; i < MH3U_DS::jewels()->size(); i++)
    {
        m_firstJewelIdentifier->addItem(QString(MH3U_DS::jewels()->at(i).identifier.c_str()), MH3U_DS::jewels()->at(i).count);
        m_secondJewelIdentifier->addItem(QString(MH3U_DS::jewels()->at(i).identifier.c_str()), MH3U_DS::jewels()->at(i).count);
        m_thirdJewelIdentifier->addItem(QString(MH3U_DS::jewels()->at(i).identifier.c_str()), MH3U_DS::jewels()->at(i).count);
    }
    configureSearchableComboBox(m_firstJewelIdentifier);
    configureSearchableComboBox(m_secondJewelIdentifier);
    configureSearchableComboBox(m_thirdJewelIdentifier);
    m_firstJewelFixed = new QCheckBox("固定/内置", this);
    m_secondJewelFixed = new QCheckBox("固定/内置", this);
    m_thirdJewelFixed = new QCheckBox("固定/内置", this);
    for (QCheckBox *fixed : {m_firstJewelFixed, m_secondJewelFixed, m_thirdJewelFixed})
        fixed->setToolTip("对应装饰珠 ID 的 bit15；勾选时写入固定/内置标记。");


    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(new QLabel(uiText("Equipment type"), this), 0, 0);
    layout->addWidget(new QLabel(uiText("Slots count"), this), 0, 1);
    layout->addWidget(new QLabel(uiText("Identifier"), this), 0, 2);
    layout->addWidget(new QLabel(uiText("First Skill's Identifier"), this), 0, 3);
    layout->addWidget(new QLabel(uiText("First Skill's Value"), this), 0, 4);
    layout->addWidget(new QLabel(uiText("Second Skill's Identifier"), this), 0, 5);
    layout->addWidget(new QLabel(uiText("Second Skill's Value"), this), 0, 6);
    layout->addWidget(new QLabel(uiText("First Jewel's Identifier"), this), 0, 7);
    layout->addWidget(new QLabel(uiText("Second Jewel's Identifier"), this), 0, 8);
    layout->addWidget(new QLabel(uiText("Third Jewel's Identifier"), this), 0, 9);
    layout->addWidget(m_equipmentType, 1, 0);
    layout->addWidget(m_slotsCount, 1, 1);
    layout->addWidget(m_identifier, 1, 2);
    layout->addWidget(m_firstSkillIdentifier, 1, 3);
    layout->addWidget(m_firstSkillValue, 1, 4);
    layout->addWidget(m_secondSkillIdentifier, 1, 5);
    layout->addWidget(m_secondSkillValue, 1, 6);
    layout->addWidget(m_firstJewelIdentifier, 1, 7);
    layout->addWidget(m_secondJewelIdentifier, 1, 8);
    layout->addWidget(m_thirdJewelIdentifier, 1, 9);
    layout->addWidget(m_firstJewelFixed, 2, 7);
    layout->addWidget(m_secondJewelFixed, 2, 8);
    layout->addWidget(m_thirdJewelFixed, 2, 9);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    connect(buttons, SIGNAL(accepted()), this, SLOT(saveAndAccept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    layout->addWidget(buttons, 3, 0, 1, 10);
    this->setLayout(layout);
    this->setWindowTitle(uiText("Single charm editor"));

    this->load();
}

void QCharm::selectOrPreserve(QComboBox *combo, uint16_t value, const QString &kind)
{
    int index = combo->findData(value);
    if (index < 0)
    {
        combo->addItem(QString("保留原值 %1（未收录%2）").arg(charmHexWord(value), kind), value);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
}

void QCharm::load()
{
    m_equipmentType->setCurrentIndex(m_equipmentType->findData(charm->equipmentType));
    m_slotsCount->setValue(charm->slotsCount);
    selectOrPreserve(m_identifier, charm->identifier, "护石");
    selectOrPreserve(m_firstSkillIdentifier, charm->firstSkillIdentifier, "技能");
    m_firstSkillValue->setValue(charm->firstSkillValue);
    selectOrPreserve(m_secondSkillIdentifier, charm->secondSkillIdentifier, "技能");
    m_secondSkillValue->setValue(charm->secondSkillValue);
    selectOrPreserve(m_firstJewelIdentifier, charm->firstJewelIdentifier & 0x7fff, "装饰珠");
    selectOrPreserve(m_secondJewelIdentifier, charm->secondJewelIdentifier & 0x7fff, "装饰珠");
    selectOrPreserve(m_thirdJewelIdentifier, charm->thirdJewelIdentifier & 0x7fff, "装饰珠");
    m_firstJewelFixed->setChecked((charm->firstJewelIdentifier & 0x8000) != 0);
    m_secondJewelFixed->setChecked((charm->secondJewelIdentifier & 0x8000) != 0);
    m_thirdJewelFixed->setChecked((charm->thirdJewelIdentifier & 0x8000) != 0);
}

void QCharm::save()
{
    charm->equipmentType = (uint8_t) searchableComboBoxCurrentData(m_equipmentType).toInt();
    charm->slotsCount = m_slotsCount->value();
    charm->identifier = (uint16_t) searchableComboBoxUnsignedValue(m_identifier, 0xffff);
    charm->firstSkillIdentifier = (uint16_t) searchableComboBoxUnsignedValue(m_firstSkillIdentifier, 0xffff);
    charm->firstSkillValue = m_firstSkillValue->value();
    charm->secondSkillIdentifier = (uint16_t) searchableComboBoxUnsignedValue(m_secondSkillIdentifier, 0xffff);
    charm->secondSkillValue = m_secondSkillValue->value();
    charm->firstJewelIdentifier = (m_firstJewelFixed->isChecked() ? 0x8000 : 0) |
        (uint16_t) searchableComboBoxUnsignedValue(m_firstJewelIdentifier, 0x7fff);
    charm->secondJewelIdentifier = (m_secondJewelFixed->isChecked() ? 0x8000 : 0) |
        (uint16_t) searchableComboBoxUnsignedValue(m_secondJewelIdentifier, 0x7fff);
    charm->thirdJewelIdentifier = (m_thirdJewelFixed->isChecked() ? 0x8000 : 0) |
        (uint16_t) searchableComboBoxUnsignedValue(m_thirdJewelIdentifier, 0x7fff);
}

void QCharm::closeEvent(QCloseEvent *)
{
    charm = NULL;
}

bool QCharm::validate()
{
    uint8_t equipmentType = (uint8_t) searchableComboBoxCurrentData(m_equipmentType).toInt();
    bool identifierOk = false;
    uint16_t identifier = (uint16_t) searchableComboBoxUnsignedValue(m_identifier, 0xffff, &identifierOk);

    if (equipmentType == MH3U_Type::NoneType)
    {
        QMessageBox::warning(this, windowTitle(), "装备类型不能为“无”。");
        return false;
    }

    if (MH3U_Armory::convertSubtype((equipment_type_e) equipmentType) != MH3U_Type::CharmSubtype)
    {
        QMessageBox::warning(this, windowTitle(), "当前窗口只能保存护石类型。");
        return false;
    }

    if (!identifierOk || identifier == 0)
    {
        QMessageBox::warning(this, windowTitle(), "编号不能为“无”。请先选择具体护石。");
        return false;
    }

    for (QComboBox *combo : {m_firstSkillIdentifier, m_secondSkillIdentifier})
    {
        bool ok = false;
        searchableComboBoxUnsignedValue(combo, 0xffff, &ok);
        if (!ok)
        {
            QMessageBox::warning(this, windowTitle(), "技能 ID 必须是 0–65535 或 0x0000–0xFFFF。");
            return false;
        }
    }
    for (QComboBox *combo : {m_firstJewelIdentifier, m_secondJewelIdentifier, m_thirdJewelIdentifier})
    {
        bool ok = false;
        searchableComboBoxUnsignedValue(combo, 0x7fff, &ok);
        if (!ok)
        {
            QMessageBox::warning(this, windowTitle(), "装饰珠 ID 必须是 0–32767；固定标记请使用下方勾选框。");
            return false;
        }
    }

    return true;
}

void QCharm::saveAndAccept()
{
    if (!validate())
    {
        return;
    }

    save();
    accept();
}
