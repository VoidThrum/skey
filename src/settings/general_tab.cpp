#include "general_tab.h"
#include "config_io.h"
#include "hotkey_edit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <KZip>

GeneralTab::GeneralTab(QWidget *parent) : QWidget(parent) { setupUI(); }

void GeneralTab::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(8);

  // ── Enum section ──
  auto *enumFrame = new QFrame(this);
  enumFrame->setFrameStyle(QFrame::StyledPanel);
  auto *enumLayout = new QFormLayout(enumFrame);
  enumLayout->setLabelAlignment(Qt::AlignRight);
  enumLayout->setSpacing(6);
  enumLayout->setContentsMargins(12, 12, 12, 12);

  inputMethodCombo_ = new QComboBox(enumFrame);
  inputMethodCombo_->addItem("Telex", "Telex");
  inputMethodCombo_->addItem("VNI", "VNI");
  enumLayout->addRow(QString::fromUtf8("Kiểu gõ:"), inputMethodCombo_);

  outputModeCombo_ = new QComboBox(enumFrame);
  outputModeCombo_->addItem("Auto", "Auto");
  outputModeCombo_->addItem("Uinput", "Uinput");
  outputModeCombo_->addItem("Surrounding Text", "Surrounding Text");
  outputModeCombo_->addItem("Preedit", "Preedit");
  enumLayout->addRow(QString::fromUtf8("Chế độ xuất:"), outputModeCombo_);

  charsetCombo_ = new QComboBox(enumFrame);
  charsetCombo_->addItem("Unicode", "Unicode");
  charsetCombo_->addItem("TCVN3 (ABC)", "TCVN3 (ABC)");
  charsetCombo_->addItem("VNI Windows", "VNI Windows");
  enumLayout->addRow(QString::fromUtf8("Bảng mã:"), charsetCombo_);

  triggerKeyEdit_ = new HotkeyEdit(enumFrame);
  triggerKeyEdit_->setToolTip(
      QString::fromUtf8("Nhấn tổ hợp phím để thay đổi"));
  enumLayout->addRow(QString::fromUtf8("Phím chuyển bộ gõ:"), triggerKeyEdit_);

  modeMenuKeyEdit_ = new HotkeyEdit(enumFrame);
  modeMenuKeyEdit_->setToolTip(
      QString::fromUtf8("Phím tắt để mở menu chế độ (mặc định: `)"));
  enumLayout->addRow(QString::fromUtf8("Phím menu chế độ:"), modeMenuKeyEdit_);

  mainLayout->addWidget(enumFrame);

  // ── Checkbox section (2 columns) ──
  auto *checkFrame = new QFrame(this);
  checkFrame->setFrameStyle(QFrame::StyledPanel);
  auto *checkLayout = new QGridLayout(checkFrame);
  checkLayout->setHorizontalSpacing(24);
  checkLayout->setVerticalSpacing(4);
  checkLayout->setContentsMargins(12, 12, 12, 12);
  checkLayout->setColumnStretch(0, 1);
  checkLayout->setColumnStretch(1, 1);

  // Telex-only options: 'w'→'ư' and '][' → 'ư'/'ơ'.
  // Enabled only when the current input method is Telex (see below).
  shortWCheck_ = new QCheckBox(QString::fromUtf8("Gõ w thành ư"), checkFrame);
  shortWCheck_->setToolTip(
      QString::fromUtf8("Chỉ Telex: gõ phím w đơn lẻ sẽ ra chữ ư."));
  checkLayout->addWidget(shortWCheck_, 0, 0);

  bracketUOCheck_ =
      new QCheckBox(QString::fromUtf8("Gõ ][ thành ư ơ"), checkFrame);
  bracketUOCheck_->setToolTip(
      QString::fromUtf8("Chỉ Telex: gõ [ ra ơ và ] ra ư (giống UniKey)."));
  checkLayout->addWidget(bracketUOCheck_, 0, 1);

  freeMarkingCheck_ =
      new QCheckBox(QString::fromUtf8("Đánh dấu tự do"), checkFrame);
  checkLayout->addWidget(freeMarkingCheck_, 1, 0);

  autoRestoreCheck_ =
      new QCheckBox(QString::fromUtf8("Tự động khôi phục"), checkFrame);
  checkLayout->addWidget(autoRestoreCheck_, 1, 1);

  showPreeditCheck_ =
      new QCheckBox(QString::fromUtf8("Hiện preedit"), checkFrame);
  checkLayout->addWidget(showPreeditCheck_, 2, 0);

  debugCheck_ = new QCheckBox(QString::fromUtf8("Ghi log debug"), checkFrame);
  checkLayout->addWidget(debugCheck_, 2, 1);

  mainLayout->addWidget(checkFrame);

  // ── Backup / Restore ──
  auto *backupFrame = new QFrame(this);
  backupFrame->setFrameStyle(QFrame::StyledPanel);
  auto *backupLayout = new QHBoxLayout(backupFrame);
  backupLayout->setContentsMargins(12, 10, 12, 10);
  backupLayout->setSpacing(8);

  auto *backupLabel =
      new QLabel(QString::fromUtf8("Backup / Restore:"), backupFrame);
  backupLayout->addWidget(backupLabel);
  backupLayout->addStretch();

  backupButton_ = new QPushButton(QString::fromUtf8("Sao lưu"), backupFrame);
  backupButton_->setMinimumWidth(90);
  connect(backupButton_, &QPushButton::clicked, this, &GeneralTab::onBackup);
  backupLayout->addWidget(backupButton_);

  restoreButton_ = new QPushButton(QString::fromUtf8("Khôi phục"), backupFrame);
  restoreButton_->setMinimumWidth(90);
  connect(restoreButton_, &QPushButton::clicked, this, &GeneralTab::onRestore);
  backupLayout->addWidget(restoreButton_);

  mainLayout->addWidget(backupFrame);
  mainLayout->addStretch();

  // Telex-only options are disabled (greyed out) unless Telex is selected.
  auto syncTelexOptions = [this]() {
    bool isTelex =
        inputMethodCombo_->currentData().toString() == QLatin1String("Telex");
    shortWCheck_->setEnabled(isTelex);
    bracketUOCheck_->setEnabled(isTelex);
  };
  connect(inputMethodCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [syncTelexOptions](int) { syncTelexOptions(); });
  syncTelexOptions();
}

void GeneralTab::loadFromConfig(const SKeyConfig &cfg) {
  auto setCombo = [](QComboBox *c, const std::string &val) {
    int idx = c->findData(QString::fromStdString(val));
    if (idx >= 0)
      c->setCurrentIndex(idx);
  };

  setCombo(inputMethodCombo_, cfg.inputMethod);
  setCombo(outputModeCombo_, cfg.outputMode);
  setCombo(charsetCombo_, cfg.charset);

  shortWCheck_->setChecked(cfg.shortW);
  bracketUOCheck_->setChecked(cfg.bracketUO);
  freeMarkingCheck_->setChecked(cfg.freeMarking);
  autoRestoreCheck_->setChecked(cfg.autoRestore);
  showPreeditCheck_->setChecked(cfg.showPreedit);

  debugCheck_->setChecked(cfg.debug);
}

SKeyConfig GeneralTab::collectConfig() const {
  SKeyConfig cfg;
  cfg.inputMethod = inputMethodCombo_->currentData().toString().toStdString();
  cfg.outputMode = outputModeCombo_->currentData().toString().toStdString();
  cfg.charset = charsetCombo_->currentData().toString().toStdString();
  cfg.shortW = shortWCheck_->isChecked();
  cfg.bracketUO = bracketUOCheck_->isChecked();
  cfg.freeMarking = freeMarkingCheck_->isChecked();
  cfg.autoRestore = autoRestoreCheck_->isChecked();
  cfg.showPreedit = showPreeditCheck_->isChecked();
  cfg.debug = debugCheck_->isChecked();
  cfg.modeMenuKey = modeMenuKeyEdit_->fcitx5Value();
  return cfg;
}

void GeneralTab::setDefaults() {
  loadFromConfig(defaultConfig());
  triggerKeyEdit_->setFcitx5Value("Control+space");
  modeMenuKeyEdit_->setFcitx5Value("grave");
}

std::string GeneralTab::triggerKey() const {
  return triggerKeyEdit_->fcitx5Value();
}

void GeneralTab::setTriggerKey(const std::string &fcitx5Key) {
  triggerKeyEdit_->setFcitx5Value(fcitx5Key);
}

std::string GeneralTab::modeMenuKey() const {
  return modeMenuKeyEdit_->fcitx5Value();
}

void GeneralTab::setModeMenuKey(const std::string &fcitx5Key) {
  modeMenuKeyEdit_->setFcitx5Value(fcitx5Key);
}

void GeneralTab::onBackup() {
  QString defaultName =
      QString::fromUtf8("skey-backup-%1.zip")
          .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
  QString savePath = QFileDialog::getSaveFileName(
      this, QString::fromUtf8("Lưu bản sao lưu cấu hình"),
      QDir::homePath() + "/" + defaultName,
      QString::fromUtf8("Tệp ZIP (*.zip)"));
  if (savePath.isEmpty())
    return;

  KZip zip(savePath);
  if (!zip.open(QIODevice::WriteOnly)) {
    QMessageBox::warning(
        this, QString::fromUtf8("Lỗi"),
        QString::fromUtf8("Không thể tạo tệp sao lưu:\n%1").arg(savePath));
    return;
  }

  struct {
    std::string path;
    const char *name;
  } files[] = {
      {skeyConfPath(), "skey.conf"},
      {appModesPath(), "skey-app-modes.conf"},
      {macroPath(), "skey-macro.conf"},
      {fcitx5ConfigPath(), "fcitx5-config"},
  };

  for (auto &f : files) {
    QFile file(QString::fromStdString(f.path));
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      file.close();
      zip.writeFile(QString::fromUtf8(f.name), data);
    }
  }

  zip.close();
  QMessageBox::information(
      this, QString::fromUtf8("Đã sao lưu"),
      QString::fromUtf8("Cấu hình đã được lưu vào:\n%1").arg(savePath));
}

void GeneralTab::onRestore() {
  auto answer = QMessageBox::question(
      this, QString::fromUtf8("Khôi phục cấu hình"),
      QString::fromUtf8("Khôi phục sẽ ghi đè toàn bộ cấu hình hiện tại.\n"
                        "Bạn có chắc muốn tiếp tục?"),
      QMessageBox::Yes | QMessageBox::No);
  if (answer != QMessageBox::Yes)
    return;

  QString openPath = QFileDialog::getOpenFileName(
      this, QString::fromUtf8("Chọn tệp sao lưu để khôi phục"),
      QDir::homePath(), QString::fromUtf8("Tệp ZIP (*.zip)"));
  if (openPath.isEmpty())
    return;

  KZip zip(openPath);
  if (!zip.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(
        this, QString::fromUtf8("Lỗi"),
        QString::fromUtf8("Không thể mở tệp sao lưu:\n%1").arg(openPath));
    return;
  }

  struct {
    const char *entryName;
    std::string destPath;
  } mappings[] = {
      {"skey.conf", skeyConfPath()},
      {"skey-app-modes.conf", appModesPath()},
      {"skey-macro.conf", macroPath()},
      {"fcitx5-config", fcitx5ConfigPath()},
  };

  bool allOk = true;
  const KArchiveDirectory *root = zip.directory();
  for (auto &m : mappings) {
    const KArchiveEntry *entry = root->entry(QString::fromUtf8(m.entryName));
    if (!entry || !entry->isFile()) {
      allOk = false;
      continue;
    }
    const KArchiveFile *file = static_cast<const KArchiveFile *>(entry);
    QByteArray data = file->data();
    QFile out(QString::fromStdString(m.destPath));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      allOk = false;
      continue;
    }
    out.write(data);
    out.close();
  }

  zip.close();

  if (!allOk) {
    QMessageBox::warning(this, QString::fromUtf8("Cảnh báo"),
                         QString::fromUtf8("Một số tệp không thể khôi phục.\n"
                                           "Kiểm tra lại tệp sao lưu."));
  }

  reloadFcitx5();
  emit configRestored();
  QMessageBox::information(
      this, QString::fromUtf8("Đã khôi phục"),
      QString::fromUtf8("Cấu hình đã được khôi phục và áp dụng."));
}
