#ifndef SKEY_SETTINGS_GENERAL_TAB_H
#define SKEY_SETTINGS_GENERAL_TAB_H

#include <QWidget>

class QComboBox;
class QCheckBox;
class QPushButton;
class HotkeyEdit;

struct SKeyConfig;

class GeneralTab : public QWidget {
    Q_OBJECT
public:
    explicit GeneralTab(QWidget *parent = nullptr);

    void loadFromConfig(const SKeyConfig &cfg);
    SKeyConfig collectConfig() const;
    void setDefaults();

    std::string triggerKey() const;
    void setTriggerKey(const std::string &fcitx5Key);

    std::string modeMenuKey() const;
    void setModeMenuKey(const std::string &fcitx5Key);

signals:
    /// Emitted after a config restore so the parent window can reload all tabs.
    void configRestored();

private slots:
    void onBackup();
    void onRestore();

private:
    void setupUI();

    QComboBox *inputMethodCombo_;
    QComboBox *outputModeCombo_;
    QComboBox *charsetCombo_;
    HotkeyEdit *triggerKeyEdit_;
    HotkeyEdit *modeMenuKeyEdit_;
    QCheckBox *shortWCheck_;
    QCheckBox *bracketUOCheck_;
    QCheckBox *freeMarkingCheck_;
    QCheckBox *autoRestoreCheck_;
    QCheckBox *showPreeditCheck_;
    QCheckBox *debugCheck_;
    QPushButton *backupButton_;
    QPushButton *restoreButton_;
};

#endif // SKEY_SETTINGS_GENERAL_TAB_H
