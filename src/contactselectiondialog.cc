#include "contactselectiondialog.hh"
#include "contact.hh"

#include <QListWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>

/* ********************************************************************************************* *
 * Implementation of MultiGroupCallSelectionDialog
 * ********************************************************************************************* */
MultiGroupCallSelectionDialog::MultiGroupCallSelectionDialog(ContactList *contacts, QWidget *parent)
  : QDialog(parent)
{
  _contacts = new QListWidget();
  for (int i=0; i<contacts->count(); i++) {
    Contact *contact = contacts->contact(i);
    if (! contact->is<DigitalContact>())
      continue;
    DigitalContact *digi = contact->as<DigitalContact>();
    if (DigitalContact::PrivateCall == digi->type())
      continue;
    QListWidgetItem *item = new QListWidgetItem(digi->name());
    item->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
    item->setData(Qt::UserRole, QVariant::fromValue(digi));
    item->setCheckState(Qt::Unchecked);
    _contacts->addItem(item);
  }

  QDialogButtonBox *bbox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
  connect(bbox, SIGNAL(accepted()), this, SLOT(accept()));
  connect(bbox, SIGNAL(rejected()), this, SLOT(reject()));

  _label = new QLabel(tr("Select a group call:"));
  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(_label);
  layout->addWidget(_contacts);
  layout->addWidget(bbox);
  setLayout(layout);
}


void
MultiGroupCallSelectionDialog::setLabel(const QString &text) {
  _label->setText(text);
}

QList<DigitalContact *>
MultiGroupCallSelectionDialog::contacts() {
  QList<DigitalContact *> contacts;
  for (int i=0; i<_contacts->count(); i++) {
    if (Qt::Checked == _contacts->item(i)->checkState())
      contacts.push_back(_contacts->item(i)->data(Qt::UserRole).value<DigitalContact *>());
  }
  return contacts;
}
