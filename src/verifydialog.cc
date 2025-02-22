#include "verifydialog.hh"
#include <QPushButton>

VerifyDialog::VerifyDialog(const QList<VerifyIssue> &issues, bool upload, QWidget *parent)
    : QDialog(parent)
{
	setupUi(this);

  bool valid = true;
  foreach (VerifyIssue issue, issues) {
    QListWidgetItem *item = new QListWidgetItem(issue.message());
    if (VerifyIssue::ERROR == issue.type()) {
      item->setForeground(Qt::red);
      valid = false;
    } else if (VerifyIssue::WARNING == issue.type()) {
      item->setForeground(Qt::black);
    } else if (VerifyIssue::NOTIFICATION == issue.type()) {
      item->setForeground(Qt::gray);
    }
    listWidget->addItem(item);
  }
  if (upload) {
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    QPushButton *ignore = buttonBox->button(QDialogButtonBox::Ok);
    ignore->setEnabled(valid);
  }
}
