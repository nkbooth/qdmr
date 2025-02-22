#include "analogchanneldialog.hh"
#include "application.hh"
#include <QCompleter>
#include "ctcssbox.hh"
#include "repeaterdatabase.hh"
#include "utils.hh"


/* ********************************************************************************************* *
 * Implementation of AnalogChannelDialog
 * ********************************************************************************************* */
AnalogChannelDialog::AnalogChannelDialog(Config *config, QWidget *parent)
  : QDialog(parent), _config(config), _channel(nullptr)
{
  construct();
}

AnalogChannelDialog::AnalogChannelDialog(Config *config, AnalogChannel *channel, QWidget *parent)
  : QDialog(parent), _config(config), _channel(channel)
{
  construct();
}

void
AnalogChannelDialog::construct() {
  setupUi(this);

  Application *app = qobject_cast<Application *>(qApp);
  FMRepeaterFilter *filter = new FMRepeaterFilter(this);
  filter->setSourceModel(app->repeater());
  QCompleter *completer = new QCompleter(filter, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setCompletionColumn(0);
  channelName->setCompleter(completer);
  connect(completer, SIGNAL(activated(const QModelIndex &)),
          this, SLOT(onRepeaterSelected(const QModelIndex &)));

  rxFrequency->setValidator(new QDoubleValidator(0,500,5));
  txFrequency->setValidator(new QDoubleValidator(0,500,5));
  powerValue->setItemData(0, unsigned(Channel::Power::Max));
  powerValue->setItemData(1, unsigned(Channel::Power::High));
  powerValue->setItemData(2, unsigned(Channel::Power::Mid));
  powerValue->setItemData(3, unsigned(Channel::Power::Low));
  powerValue->setItemData(4, unsigned(Channel::Power::Min));
  powerDefault->setChecked(true); powerValue->setEnabled(false); powerValue->setCurrentIndex(1);
  totDefault->setChecked(true); totValue->setValue(0); totValue->setEnabled(false);
  scanList->addItem(tr("[None]"), QVariant::fromValue((ScanList *)nullptr));
  scanList->setCurrentIndex(0);
  for (int i=0; i<_config->scanlists()->count(); i++) {
    ScanList *lst = _config->scanlists()->scanlist(i);
    scanList->addItem(lst->name(),QVariant::fromValue(lst));
    if (_channel && (_channel->scanListObj() == lst) )
      scanList->setCurrentIndex(i+1);
  }
  txAdmit->setItemData(0, unsigned(AnalogChannel::Admit::Always));
  txAdmit->setItemData(1, unsigned(AnalogChannel::Admit::Free));
  txAdmit->setItemData(2, unsigned(AnalogChannel::Admit::Tone));
  squelchDefault->setChecked(true); squelchValue->setValue(1); squelchValue->setEnabled(false);
  populateCTCSSBox(rxTone, (nullptr != _channel ? _channel->rxTone() : Signaling::SIGNALING_NONE));
  populateCTCSSBox(txTone, (nullptr != _channel ? _channel->txTone() : Signaling::SIGNALING_NONE));
  bandwidth->setItemData(0, unsigned(AnalogChannel::Bandwidth::Narrow));
  bandwidth->setItemData(1, unsigned(AnalogChannel::Bandwidth::Wide));
  aprsList->addItem(tr("[None]"), QVariant::fromValue((APRSSystem *)nullptr));
  aprsList->setCurrentIndex(0);
  for (int i=0; i<_config->posSystems()->aprsCount(); i++) {
    APRSSystem *sys = _config->posSystems()->aprsSystem(i);
    aprsList->addItem(sys->name(),QVariant::fromValue(sys));
    if (_channel && (_channel->aprsSystem() == sys))
      aprsList->setCurrentIndex(i+1);
  }
  voxDefault->setChecked(true); voxValue->setValue(0); voxValue->setEnabled(false);

  if (_channel) {
    channelName->setText(_channel->name());
    rxFrequency->setText(format_frequency(_channel->rxFrequency()));
    txFrequency->setText(format_frequency(_channel->txFrequency()));
    if (! _channel->defaultPower()) {
      powerDefault->setChecked(false); powerValue->setEnabled(true);
      switch (_channel->power()) {
      case Channel::Power::Max: powerValue->setCurrentIndex(0); break;
      case Channel::Power::High: powerValue->setCurrentIndex(1); break;
      case Channel::Power::Mid: powerValue->setCurrentIndex(2); break;
      case Channel::Power::Low: powerValue->setCurrentIndex(3); break;
      case Channel::Power::Min: powerValue->setCurrentIndex(4); break;
      }
    }
    if (! _channel->defaultTimeout()) {
      totDefault->setChecked(false); totValue->setEnabled(true);
      totValue->setValue(_channel->timeout());
    }
    rxOnly->setChecked(_channel->rxOnly());
    switch (_channel->admit()) {
      case AnalogChannel::Admit::Always: txAdmit->setCurrentIndex(0); break;
      case AnalogChannel::Admit::Free: txAdmit->setCurrentIndex(1); break;
      case AnalogChannel::Admit::Tone: txAdmit->setCurrentIndex(2); break;
    }
    if (! _channel->defaultSquelch()) {
      squelchDefault->setChecked(false); squelchValue->setEnabled(true);
      squelchValue->setValue(_channel->squelch());
    }
    if (AnalogChannel::Bandwidth::Narrow == _channel->bandwidth())
      bandwidth->setCurrentIndex(0);
    else if (AnalogChannel::Bandwidth::Wide == _channel->bandwidth())
      bandwidth->setCurrentIndex(1);
    if (! _channel->defaultVOX()) {
      voxDefault->setChecked(false); voxValue->setEnabled(true);
      voxValue->setValue(_channel->vox());
    }
  }

  connect(powerDefault, SIGNAL(toggled(bool)), this, SLOT(onPowerDefaultToggled(bool)));
  connect(totDefault, SIGNAL(toggled(bool)), this, SLOT(onTimeoutDefaultToggled(bool)));
  connect(squelchDefault, SIGNAL(toggled(bool)), this, SLOT(onSquelchDefaultToggled(bool)));
  connect(voxDefault, SIGNAL(toggled(bool)), this, SLOT(onVOXDefaultToggled(bool)));
  connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
  connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

AnalogChannel *
AnalogChannelDialog::channel()
{
  AnalogChannel *channel = nullptr;
  if (_channel) {
    channel = _channel;
  } else {
    channel = new AnalogChannel();
  }

  channel->setName(channelName->text());
  channel->setRXFrequency(rxFrequency->text().toDouble());
  channel->setTXFrequency(txFrequency->text().toDouble());
  if (powerDefault->isChecked()) {
    channel->setDefaultPower();
  } else {
    channel->setPower(Channel::Power(powerValue->currentData().toUInt()));
  }
  if (totDefault->isChecked())
    channel->setDefaultTimeout();
  else
    channel->setTimeout(totValue->value());
  channel->setRXOnly(rxOnly->isChecked());
  channel->setAdmit(AnalogChannel::Admit(txAdmit->currentData().toUInt()));
  if (squelchDefault->isChecked())
    channel->setSquelchDefault();
  else
    channel->setSquelch(squelchValue->value());
  channel->setRXTone(Signaling::Code(rxTone->currentData().toUInt()));
  channel->setTXTone(Signaling::Code(txTone->currentData().toUInt()));
  channel->setBandwidth(AnalogChannel::Bandwidth(bandwidth->currentData().toUInt()));
  channel->setScanListObj(scanList->currentData().value<ScanList *>());
  channel->setAPRSSystem(aprsList->currentData().value<APRSSystem *>());
  if (voxDefault->isChecked())
    channel->setVOXDefault();
  else
    channel->setVOX(voxValue->value());

  return channel;
}

void
AnalogChannelDialog::onRepeaterSelected(const QModelIndex &index) {
  Application *app = qobject_cast<Application *>(qApp);

  QModelIndex src = qobject_cast<QAbstractProxyModel*>(
        channelName->completer()->completionModel())->mapToSource(index);
  src = qobject_cast<QAbstractProxyModel*>(
        channelName->completer()->model())->mapToSource(src);
  double rx = app->repeater()->repeater(src.row()).value("tx").toDouble();
  double tx = app->repeater()->repeater(src.row()).value("rx").toDouble();
  txFrequency->setText(QString::number(tx, 'f'));
  rxFrequency->setText(QString::number(rx, 'f'));
}

void
AnalogChannelDialog::onPowerDefaultToggled(bool checked) {
  powerValue->setEnabled(!checked);
}

void
AnalogChannelDialog::onTimeoutDefaultToggled(bool checked) {
  totValue->setEnabled(!checked);
}

void
AnalogChannelDialog::onSquelchDefaultToggled(bool checked) {
  squelchValue->setEnabled(! checked);
}

void
AnalogChannelDialog::onVOXDefaultToggled(bool checked) {
  voxValue->setEnabled(! checked);
}

