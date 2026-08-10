#include "qcharacter.hpp"

#include <QGridLayout>
#include <QLabel>
QCharacter::QCharacter(MH3U_SE *mh3u, QWidget *parent) : QDialog(parent)
{
    this->mh3u = mh3u;

    m_sexs = new QComboBox(this);
    m_sexs->addItem("男", 0);
    m_sexs->addItem("女", 1);
    configureSearchableComboBox(m_sexs);

    m_faces = new QComboBox(this);
    for (int i = 0; i <= 31; ++i)
        m_faces->addItem(QString("%1").arg(i), i);
    configureSearchableComboBox(m_faces);

    m_hairs = new QComboBox(this);
    for (int i = 0; i <= 63; ++i)
        m_hairs->addItem(QString("%1").arg(i), i);
    configureSearchableComboBox(m_hairs);

    m_name = new QLineEdit(this);
    m_name->setMaxLength(mh3u->nameSize());
    m_money = new QSpinBox(this);
    m_money->setMinimum(0x0000000);
    m_money->setMaximum(0xfffffff);

    m_voices = new QComboBox(this);
    for (int i = 0; i <= 31; ++i)
        m_voices->addItem(QString("%1").arg(i), i);
    configureSearchableComboBox(m_voices);

    m_mogapoint = new QSpinBox(this);
    m_mogapoint->setMinimum(0x0000000);
    m_mogapoint->setMaximum(0xfffffff);


    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(new QLabel(uiText("Sex"), this), 0, 0);
    layout->addWidget(new QLabel("内衣样式", this), 0, 1);
    layout->addWidget(new QLabel(uiText("Hairstyle"), this), 0, 2);
    layout->addWidget(new QLabel(uiText("Name"), this), 0, 3);
    layout->addWidget(new QLabel(uiText("Money"), this), 0, 4);
    layout->addWidget(new QLabel(uiText("Voice"), this), 0, 5);
    layout->addWidget(new QLabel("猎人等级 HR", this), 0, 6);
    layout->addWidget(m_sexs, 1, 0);
    layout->addWidget(m_faces, 1, 1);
    layout->addWidget(m_hairs, 1, 2);
    layout->addWidget(m_name, 1, 3);
    layout->addWidget(m_money, 1, 4);
    layout->addWidget(m_voices, 1, 5);
    layout->addWidget(m_mogapoint, 1, 6);
    this->setLayout(layout);
    this->setWindowTitle(uiText("Character data editor"));

    this->load();
}

QCharacter::~QCharacter()
{
    this->mh3u = NULL;
}


void QCharacter::load()
{
    m_sexs->setCurrentIndex(m_sexs->findData(mh3u->savedata->sex));
    m_faces->setCurrentIndex(m_faces->findData(mh3u->savedata->face));
    m_hairs->setCurrentIndex(m_hairs->findData(mh3u->savedata->hair));
    int nameLength = 0;
    while (nameLength < 12 && mh3u->savedata->name[nameLength] != 0) ++nameLength;
    m_name->setText(QString::fromUtf16(mh3u->savedata->name, nameLength));
    m_money->setValue(mh3u->savedata->money);
    m_voices->setCurrentIndex(m_voices->findData(mh3u->savedata->voice));
    m_mogapoint->setValue(mh3u->savedata->mogapoint);
}

void QCharacter::save()
{
    mh3u->savedata->sex = searchableComboBoxCurrentData(m_sexs).toInt();
    mh3u->savedata->face = searchableComboBoxCurrentData(m_faces).toInt();
    mh3u->savedata->hair = searchableComboBoxCurrentData(m_hairs).toInt();
    const QString name = m_name->text().left(11);
    std::memset(mh3u->savedata->name, 0, sizeof(name_t));
    for (int index = 0; index < name.size(); ++index)
        mh3u->savedata->name[index] = name.at(index).unicode();
    mh3u->savedata->money = m_money->value();
    mh3u->savedata->voice = searchableComboBoxCurrentData(m_voices).toInt();
    mh3u->savedata->mogapoint = m_mogapoint->value();
}

void QCharacter::closeEvent(QCloseEvent *)
{
    save();
}
