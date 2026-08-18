#include "searchable_combobox.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QLineEdit>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

void processPopup()
{
    QApplication::processEvents();
    QTest::qWait(30);
    QApplication::processEvents();
}
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    try
    {
        QWidget window;
        QVBoxLayout layout(&window);
        QComboBox combo(&window);
        QLineEdit other(&window);
        combo.addItem(QString::fromUtf8("雷狼龙 (Zinogre)"), 101);
        combo.addItem(QString::fromUtf8("爆锤龙 (Uragaan)"), 202);
        combo.addItem(QString::fromUtf8("重复名称 (Duplicate)"), 301);
        combo.addItem(QString::fromUtf8("重复名称 (Duplicate)"), 302);
        configureSearchableComboBox(&combo);
        combo.setCurrentIndex(1);
        layout.addWidget(&combo);
        layout.addWidget(&other);
        window.show();
        processPopup();

        QLineEdit *editor = combo.lineEdit();
        QCompleter *completer = editor == NULL ? NULL : editor->completer();
        require(editor != NULL && completer != NULL, "search editor or completer was not configured");

        QTest::mouseClick(editor, Qt::LeftButton);
        processPopup();
        require(editor->hasFocus(), "clicking the editor moved focus away");
        require(!combo.view()->isVisible(), "clicking the editor forced the full popup open");

        editor->selectAll();
        QTest::keyClicks(editor, "zINo");
        processPopup();
        require(editor->hasFocus(), "typing moved focus away");
        require(completer->completionModel()->rowCount() == 1, "case-insensitive contains filtering failed");
        require(completer->completionModel()->index(0, 0).data(Qt::UserRole).toInt() == 101,
                "English filtering returned the wrong item data");
        completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
        QTest::keyClick(editor, Qt::Key_Return);
        processPopup();
        require(searchableComboBoxCurrentData(&combo).toInt() == 101,
                "Enter did not commit the highlighted completion");

        completer->setCompletionPrefix(QString::fromUtf8("雷狼"));
        require(completer->completionModel()->rowCount() == 1, "Chinese contains filtering failed");

        editor->selectAll();
        QTest::keyClicks(editor, "duplicate");
        processPopup();
        require(completer->completionModel()->rowCount() == 2, "duplicate-name candidates were collapsed");
        const QModelIndex duplicate = completer->completionModel()->index(1, 0);
        require(duplicate.data(Qt::UserRole).toInt() == 302, "duplicate candidate lost its real ID");
        completer->popup()->setCurrentIndex(duplicate);
        QTest::mouseClick(completer->popup()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          completer->popup()->visualRect(duplicate).center());
        processPopup();
        require(searchableComboBoxCurrentData(&combo).toInt() == 302,
                "clicking a duplicate candidate committed the wrong ID");

        int selectionChanges = 0;
        QObject::connect(&combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                         [&selectionChanges](int) { ++selectionChanges; });
        editor->selectAll();
        QTest::keyClicks(editor, "missing candidate");
        processPopup();
        require(selectionChanges == 0, "temporary search text changed the business selection");
        other.setFocus();
        processPopup();
        require(searchableComboBoxCurrentData(&combo).toInt() == 302,
                "focus loss replaced the committed item data");
        require(combo.currentText() == QString::fromUtf8("重复名称 (Duplicate)"),
                "focus loss did not restore the committed display text");
        require(selectionChanges == 0, "focus-loss restoration emitted a selection change");

        std::cout << "searchable combo interaction tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
