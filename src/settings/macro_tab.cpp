#include "macro_tab.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

// Security limits
static constexpr size_t kMaxMacroKeyLen   = 32;
static constexpr size_t kMaxMacroValueLen = 256;

static bool isValidMacroKey(const std::string &key) {
    if (key.empty()) return false;
    for (unsigned char c : key) {
        if (!std::isalnum(c)) return false;
    }
    return true;
}

MacroTab::MacroTab(QWidget *parent) : QWidget(parent) {
    setupUI();
}

void MacroTab::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ── Checkboxes ──
    auto *checkFrame = new QGroupBox(QString::fromUtf8("Tùy chọn"), this);
    auto *checkLayout = new QHBoxLayout(checkFrame);
    checkLayout->setContentsMargins(12, 8, 12, 8);
    checkLayout->setSpacing(16);

    enableCheck_ = new QCheckBox(QString::fromUtf8("Bật gõ tắt"), checkFrame);
    enableCheck_->setToolTip(
        QString::fromUtf8("Bật/tắt toàn bộ tính năng gõ tắt."));
    checkLayout->addWidget(enableCheck_);

    capitalizeCheck_ = new QCheckBox(
        QString::fromUtf8("Viết hoa macro"), checkFrame);
    capitalizeCheck_->setToolTip(
        QString::fromUtf8("Nếu từ viết tắt có chữ cái đầu viết hoa,\n"
                          "kết quả cũng sẽ được viết hoa chữ cái đầu."));
    checkLayout->addWidget(capitalizeCheck_);

    offModeCheck_ = new QCheckBox(
        QString::fromUtf8("Gõ tắt khi VN off"), checkFrame);
    offModeCheck_->setToolTip(
        QString::fromUtf8("Khi bật, gõ tắt vẫn hoạt động ngay cả\n"
                          "khi chế độ gõ tiếng Việt đang tắt."));
    checkLayout->addWidget(offModeCheck_);

    mainLayout->addWidget(checkFrame);

    // ── Table ──
    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({
        QString::fromUtf8("Từ viết tắt"),
        QString::fromUtf8("Thành"),
        QString::fromUtf8("Xóa"),
    });
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setMinimumHeight(200);
    mainLayout->addWidget(table_);

    // ── Input row ──
    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);

    inputRow->addWidget(new QLabel(QString::fromUtf8("Từ vt:"), this));
    keyEdit_ = new QLineEdit(this);
    keyEdit_->setPlaceholderText(QString::fromUtf8("vd: dc"));
    keyEdit_->setMaxLength(static_cast<int>(kMaxMacroKeyLen));
    inputRow->addWidget(keyEdit_);

    inputRow->addWidget(new QLabel(QString::fromUtf8("Thành:"), this));
    valueEdit_ = new QLineEdit(this);
    valueEdit_->setPlaceholderText(QString::fromUtf8("vd: được"));
    valueEdit_->setMaxLength(static_cast<int>(kMaxMacroValueLen));
    inputRow->addWidget(valueEdit_);

    addButton_ = new QPushButton(QString::fromUtf8("Thêm"), this);
    addButton_->setMinimumWidth(80);
    connect(addButton_, &QPushButton::clicked, this, &MacroTab::onAdd);
    inputRow->addWidget(addButton_);

    mainLayout->addLayout(inputRow);
    mainLayout->addStretch();
}

void MacroTab::addRow(const std::string &key, const std::string &value) {
    int row = table_->rowCount();
    table_->insertRow(row);

    auto *keyItem = new QTableWidgetItem(QString::fromStdString(key));
    keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
    table_->setItem(row, 0, keyItem);

    auto *valItem = new QTableWidgetItem(QString::fromStdString(value));
    valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
    table_->setItem(row, 1, valItem);

    auto *delBtn = new QPushButton(QString::fromUtf8("✕"), this);
    delBtn->setFixedSize(28, 28);
    delBtn->setToolTip(QString::fromUtf8("Xóa macro này"));
    delBtn->setStyleSheet(
        "QPushButton { color: #c0392b; border: none; font-weight: bold; }"
        "QPushButton:hover { background: #c0392b; color: white; border-radius: 3px; }");
    connect(delBtn, &QPushButton::clicked, this, &MacroTab::onDelete);
    table_->setCellWidget(row, 2, delBtn);
}

void MacroTab::onAdd() {
    std::string key   = keyEdit_->text().trimmed().toStdString();
    std::string value = valueEdit_->text().trimmed().toStdString();

    if (key.empty() || value.empty()) {
        QMessageBox::warning(this,
            QString::fromUtf8("Thiếu thông tin"),
            QString::fromUtf8("Vui lòng nhập cả từ viết tắt và kết quả."));
        return;
    }
    if (key.size() > kMaxMacroKeyLen) {
        QMessageBox::warning(this,
            QString::fromUtf8("Từ viết tắt quá dài"),
            QString::fromUtf8("Từ viết tắt tối đa %1 ký tự.")
                .arg(static_cast<int>(kMaxMacroKeyLen)));
        return;
    }
    if (!isValidMacroKey(key)) {
        QMessageBox::warning(this,
            QString::fromUtf8("Ký tự không hợp lệ"),
            QString::fromUtf8("Từ viết tắt chỉ được chứa chữ cái và số.\n"
                              "Không được dùng ký tự đặc biệt hoặc khoảng trắng."));
        return;
    }
    if (value.size() > kMaxMacroValueLen) {
        QMessageBox::warning(this,
            QString::fromUtf8("Kết quả quá dài"),
            QString::fromUtf8("Kết quả tối đa %1 ký tự.")
                .arg(static_cast<int>(kMaxMacroValueLen)));
        return;
    }
    // Check for duplicate key
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (table_->item(row, 0)->text().toStdString() == key) {
            QMessageBox::warning(this,
                QString::fromUtf8("Đã tồn tại"),
                QString::fromUtf8("Từ viết tắt '%1' đã có trong danh sách.\n"
                                  "Hãy xóa entry cũ trước khi thêm lại.")
                    .arg(QString::fromStdString(key)));
            return;
        }
    }

    addRow(key, value);
    keyEdit_->clear();
    valueEdit_->clear();
    keyEdit_->setFocus();
}

void MacroTab::onDelete() {
    int row = -1;
    // Find which delete button was pressed
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto *btn = qobject_cast<QPushButton *>(table_->cellWidget(r, 2));
        if (btn && btn == sender()) { row = r; break; }
    }
    // Fallback: use selection
    if (row < 0) {
        auto selected = table_->selectionModel()->selectedRows();
        if (!selected.isEmpty()) row = selected.first().row();
    }
    if (row >= 0) table_->removeRow(row);
}

void MacroTab::loadFromConfig(const MacroTabData &data) {
    enableCheck_->setChecked(data.enableMacro);
    capitalizeCheck_->setChecked(data.capitalizeMacro);
    offModeCheck_->setChecked(data.macroInOffMode);

    table_->setRowCount(0);
    for (auto &[key, value] : data.entries) {
        addRow(key, value);
    }
}

MacroTabData MacroTab::collectConfig() const {
    MacroTabData data;
    data.enableMacro     = enableCheck_->isChecked();
    data.capitalizeMacro = capitalizeCheck_->isChecked();
    data.macroInOffMode  = offModeCheck_->isChecked();

    for (int row = 0; row < table_->rowCount(); ++row) {
        std::string key   = table_->item(row, 0)->text().toStdString();
        std::string value = table_->item(row, 1)->text().toStdString();
        if (!key.empty() && !value.empty()) {
            data.entries.emplace_back(key, value);
        }
    }
    return data;
}

void MacroTab::setDefaults() {
    MacroTabData defaults;
    loadFromConfig(defaults);
}
