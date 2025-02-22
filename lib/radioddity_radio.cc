#include "radioddity_radio.hh"
#include "config.hh"
#include "logger.hh"
#include "utils.hh"

#define BSIZE           32


RadioddityRadio::RadioddityRadio(RadioddityInterface *device, QObject *parent)
  : Radio(parent), _dev(device), _codeplugFlags(), _config(nullptr)
{
  if (! connect())
    return;
}

RadioddityRadio::~RadioddityRadio() {
  if (_dev && _dev->isOpen()) {
    logDebug() << "Reboot and close connection to radio.";
    _dev->reboot();
    _dev->close();
  }
  if (_dev) {
    _dev->deleteLater();
    _dev = nullptr;
  }
}

bool
RadioddityRadio::startDownload(bool blocking) {
  if (StatusIdle != _task)
    return false;

  _task = StatusDownload;

  if (blocking) {
    run();
    return (StatusIdle == _task);
  }

  start();
  return true;
}

bool
RadioddityRadio::startUpload(Config *config, bool blocking, const Codeplug::Flags &flags) {
  if (StatusIdle != _task)
    return false;

  if (! (_config = config))
    return false;

  _task = StatusUpload;
  _codeplugFlags = flags;
  if (blocking) {
    this->run();
    return (StatusIdle == _task);
  }

  this->start();
  return true;
}

bool
RadioddityRadio::startUploadCallsignDB(UserDatabase *db, bool blocking, const CallsignDB::Selection &selection) {
  Q_UNUSED(db);
  Q_UNUSED(blocking);
  Q_UNUSED(selection);

  errMsg() << "Radio does not support a callsign DB.";

  return false;
}

void
RadioddityRadio::run() {
  if (StatusDownload == _task) {
    if (! connect()) {
      emit downloadError(this);
      return;
    }

    if (! download()) {
      _dev->read_finish();
      _dev->reboot();
      _dev->close();
      _task = StatusError;
      emit downloadError(this);
      return;
    }

    _task = StatusIdle;
    _dev->reboot();
    _dev->close();
    emit downloadFinished(this, &codeplug());
    _config = nullptr;
  } else if (StatusUpload == _task) {
    if (! connect()) {
      emit uploadError(this);
      return;
    }

    if (! upload()) {
      _dev->write_finish();
      _dev->reboot();
      _dev->close();
      _task = StatusError;
      emit uploadError(this);
      return;
    }
    _dev->write_finish();
    _dev->reboot();
    _dev->close();
    _task = StatusIdle;
    emit uploadComplete(this);
  } else if (StatusUploadCallsigns == _task) {
    if (! connect()) {
      emit uploadError(this);
      return;
    }

    if(! uploadCallsigns()) {
      _dev->reboot();
      _dev->close();
      _task = StatusError;
      emit uploadError(this);
      return;
    }

    _task = StatusIdle;
    _dev->reboot();
    _dev->close();
    emit uploadComplete(this);

  }
}

bool
RadioddityRadio::connect() {
  // If connected -> done
  if (_dev && _dev->isOpen())
    return true;
  // If not open -> reconnect
  if (_dev)
    _dev->deleteLater();
  _dev = new RadioddityInterface(0x15a2, 0x0073);
  if (! _dev->isOpen()) {
    pushErrorMessage(_dev->errorMessages());
    errMsg() << "Cannot connect to RD5R.";
    _dev->deleteLater();
    _dev = nullptr;
    _task = StatusError;
    return false;
  }
  return true;
}

bool
RadioddityRadio::download() {
  emit downloadStarted();

  unsigned btot = 0;
  for (int n=0; n<codeplug().image(0).numElements(); n++) {
    btot += codeplug().image(0).element(n).data().size()/BSIZE;
  }

  unsigned bcount = 0;
  for (int n=0; n<codeplug().image(0).numElements(); n++) {
    int b0 = codeplug().image(0).element(n).address()/BSIZE;
    int nb = codeplug().image(0).element(n).data().size()/BSIZE;
    for (int i=0; i<nb; i++, bcount++) {
      // Select bank by addr
      uint32_t addr = (b0+i)*BSIZE;
      RadioddityInterface::MemoryBank bank = (
            (0x10000 > addr) ? RadioddityInterface::MEMBANK_CODEPLUG_LOWER : RadioddityInterface::MEMBANK_CODEPLUG_UPPER );
      // read
      if (! _dev->read(bank, (b0+i)*BSIZE, codeplug().data((b0+i)*BSIZE), BSIZE)) {
        pushErrorMessage(_dev->errorMessages());
        errMsg() << "Cannot download codeplug.";
        return false;
      }
      emit downloadProgress(float(bcount*100)/btot);
    }
  }

  _dev->read_finish();
  return true;
}

bool
RadioddityRadio::upload() {
  emit uploadStarted();

  unsigned btot = 0;
  for (int n=0; n<codeplug().image(0).numElements(); n++) {
    btot += codeplug().image(0).element(n).data().size()/BSIZE;
  }

  unsigned bcount = 0;
  if (_codeplugFlags.updateCodePlug) {
    // If codeplug gets updated, download codeplug from device first:
    for (int n=0; n<codeplug().image(0).numElements(); n++) {
      int b0 = codeplug().image(0).element(n).address()/BSIZE;
      int nb = codeplug().image(0).element(n).data().size()/BSIZE;
      for (int i=0; i<nb; i++, bcount++) {
        // Select bank by addr
        uint32_t addr = (b0+i)*BSIZE;
        RadioddityInterface::MemoryBank bank = (
              (0x10000 > addr) ? RadioddityInterface::MEMBANK_CODEPLUG_LOWER : RadioddityInterface::MEMBANK_CODEPLUG_UPPER );
        // read
        if (! _dev->read(bank, addr, codeplug().data(addr), BSIZE)) {
          pushErrorMessage(_dev->errorMessages());
          errMsg() << "Cannot upload codeplug.";
          return false;
        }
        emit uploadProgress(float(bcount*50)/btot);
      }
    }
  }

  // Encode config into codeplug
  if (! codeplug().encode(_config, _codeplugFlags)) {
    pushErrorMessage(codeplug().errorMessages());
    errMsg() << "Codeplug upload failed.";
    return false;
  }

  // then, upload modified codeplug
  bcount = 0;
  for (int n=0; n<codeplug().image(0).numElements(); n++) {
    int b0 = codeplug().image(0).element(n).address()/BSIZE;
    int nb = codeplug().image(0).element(n).data().size()/BSIZE;
    for (int i=0; i<nb; i++, bcount++) {
      // Select bank by addr
      uint32_t addr = (b0+i)*BSIZE;
      RadioddityInterface::MemoryBank bank = (
            (0x10000 > addr) ? RadioddityInterface::MEMBANK_CODEPLUG_LOWER : RadioddityInterface::MEMBANK_CODEPLUG_UPPER );
      // write block
      if (! _dev->write(bank, addr, codeplug().data(addr), BSIZE)) {
        pushErrorMessage(_dev->errorMessages());
        errMsg() << "Cannot upload codeplug.";
        return false;
      }
      emit uploadProgress(50+float(bcount*50)/btot);
    }
  }

  return true;
}

bool
RadioddityRadio::uploadCallsigns() {
  return false;
}
