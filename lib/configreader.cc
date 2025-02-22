#include "configreader.hh"
#include "config.hh"
#include "logger.hh"
#include "utils.hh"
#include <QMetaProperty>
#include <QMetaEnum>

inline QStringList
enumKeys(const QMetaEnum &e) {
  QStringList lst;
  for (int i=0; i<e.keyCount(); i++)
    lst.push_back(e.key(i));
  return lst;
}


/* ********************************************************************************************* *
 * Implementation of ConfigObjectReader
 * ********************************************************************************************* */
AbstractConfigReader::AbstractConfigReader(QObject *parent)
  : QObject(parent), _errorMessage()
{
  // pass...
}

const QString &
AbstractConfigReader::errorMessage() const {
  return _errorMessage;
}

bool
AbstractConfigReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx)
{
  Q_UNUSED(ctx)

  const QMetaObject *meta = obj->metaObject();

  for (int p=QObject::staticMetaObject.propertyOffset(); p<meta->propertyCount(); p++) {
    QMetaProperty prop = meta->property(p);
    if (! prop.isValid())
      continue;

    if (prop.isEnumType()) {
      // If property is not set -> skip
      if (! node[prop.name()])
        continue;
      // parse & check enum key
      if (! node[prop.name()].IsScalar()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected enum key.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(prop.name()).arg(meta->className());
        return false;
      }
      QMetaEnum e = prop.enumerator();
      std::string key = node[prop.name()].as<std::string>();
      bool ok=true; int value = e.keyToValue(key.c_str(), &ok);
      if (! ok) {
        _errorMessage = tr("%1:%2: Unknown key '%3' for enum '%4'. Expected one of %5.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(key.c_str()).arg(prop.name()).arg(enumKeys(e).join(", "));
        return false;
      }
      // finally set property
      prop.write(obj, value);
    } else if (QString("bool") == prop.typeName()) {
      // If property is not set -> skip
      if (! node[prop.name()])
        continue;
      // parse & check type
      if (! node[prop.name()].IsScalar()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected boolean value.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(prop.name()).arg(meta->className());
        return false;
      }
      prop.write(obj, node[prop.name()].as<bool>());
    } else if (QString("int") == prop.typeName()) {
      // If property is not set -> skip
      if (! node[prop.name()])
        continue;
      // parse & check type
      if (! node[prop.name()].IsScalar()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected integer value.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(prop.name()).arg(meta->className());
        return false;
      }
      prop.write(obj, node[prop.name()].as<int>());
    } else if (QString("uint") == prop.typeName()) {
      // If property is not set -> skip
      if (! node[prop.name()])
        continue;
      // parse & check type
      if (! node[prop.name()].IsScalar()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected unsigned integer value.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(prop.name()).arg(meta->className());
        return false;
      }
      prop.write(obj, node[prop.name()].as<unsigned>());
    } else if (QString("double") == prop.typeName()) {
      // If property is not set -> skip
      if (! node[prop.name()])
        continue;
      // parse & check type
      if (! node[prop.name()].IsScalar()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected double value.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(prop.name()).arg(meta->className());
        return false;
      }
      prop.write(obj, node[prop.name()].as<double>());
    } else if (QString("QString") == prop.typeName()) {
      // If property is not set -> skip
      if (! node[prop.name()])
        continue;
      // parse & check type
      if (! node[prop.name()].IsScalar()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected string.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(prop.name()).arg(meta->className());
        return false;
      }
      prop.write(obj, QString::fromStdString(node[prop.name()].as<std::string>()));
    } else if (prop.read(obj).value<ConfigObjectRefList *>()) {
      // reference lists are linked later
      continue;
    } else if (prop.read(obj).value<ConfigObjectReference *>()) {
      // references are linked later
      continue;
    } else {
      logDebug() << "Unhandled property " << prop.name()
                 << " of unhandled type " << prop.typeName() << ".";
    }
  }

  return true;
}

bool
AbstractConfigReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  const QMetaObject *meta = obj->metaObject();

  for (int p=QObject::staticMetaObject.propertyOffset(); p<meta->propertyCount(); p++) {
    QMetaProperty prop = meta->property(p);
    if (! prop.isValid())
      continue;
    QVariant val = prop.read(obj);
    if (val.value<ConfigObjectRefList *>()) {
      // If not set -> skip
      if (! node[prop.name()])
        continue;
      // check type
      if (! node[prop.name()].IsSequence()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected sequence.")
            .arg(node[prop.name()].Mark().line).arg(node[prop.name()].Mark().column)
            .arg(prop.name()).arg(meta->className());
        return false;
      }
      YAML::Node list = node[prop.name()];
      ConfigObjectRefList *refs = prop.read(obj).value<ConfigObjectRefList *>();
      for (YAML::const_iterator it=list.begin(); it!=list.end(); it++) {
        if (! it->IsScalar()) {
          _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected ID string.")
              .arg(it->Mark().line).arg(it->Mark().column)
              .arg(prop.name()).arg(meta->className());
          return false;
        }
        // check for tags
        QString tag = QString::fromStdString(it->Tag());
        if ((!it->Scalar().size()) && (!tag.isEmpty())) {
          if (0>refs->add(ctx.getTag(prop.enclosingMetaObject()->className(), prop.name(), tag))) {
            _errorMessage = tr("%1:%2: Cannot set %3 for %4 of %5.")
                .arg(it->Mark().line).arg(it->Mark().column).arg(tag).arg(prop.name()).arg(meta->className());
            logError() << _errorMessage;
            return false;
          }
          continue;
        }
        QString id = QString::fromStdString(it->as<std::string>());
        if (! ctx.contains(id)) {
          _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Reference %5 is not defined.")
              .arg(it->Mark().line).arg(it->Mark().column)
              .arg(prop.name()).arg(meta->className()).arg(id);
          return false;
        }
        if (0 > refs->add(ctx.getObj(id))) {
          _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Cannot add reference %5 to list.")
              .arg(it->Mark().line).arg(it->Mark().column)
              .arg(prop.name()).arg(meta->className()).arg(id);
          return false;
        }
      }
    } else if (val.value<ConfigObjectReference *>()) {
      YAML::Node el = node[prop.name()];
      // If not set -> skip
      if (! el)
        continue;
      // check type
      if (! el.IsScalar()) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Expected ID string.")
            .arg(el.Mark().line).arg(el.Mark().column) .arg(prop.name()).arg(meta->className());
        logError() << _errorMessage;
        return false;
      }
      // get reference
      ConfigObjectReference *ref = prop.read(obj).value<ConfigObjectReference *>();
      // check for tags
      QString tag = QString::fromStdString(el.Tag());
      if ((!el.Scalar().size()) && (!tag.isEmpty())) {
        if (! ref->set(ctx.getTag(prop.enclosingMetaObject()->className(), prop.name(), tag))) {
          _errorMessage = tr("%1:%2: Cannot set %3 for %4 of %5.")
              .arg(el.Mark().line).arg(el.Mark().column).arg(tag).arg(prop.name()).arg(meta->className());
          logError() << _errorMessage;
          return false;
        }
        continue;
      }
      // get & check ID
      QString id = QString::fromStdString(el.as<std::string>());
      if (! ctx.contains(id)) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Reference '%5' is not defined.")
            .arg(el.Mark().line).arg(el.Mark().column).arg(prop.name()).arg(meta->className()).arg(id);
        logError() << _errorMessage;
        return false;
      }
      // set reference
      if (! ref->set(ctx.getObj(id))) {
        _errorMessage = tr("%1:%2: Cannot parse %3 of %4: Cannot set reference %5.")
            .arg(el.Mark().line).arg(el.Mark().column).arg(prop.name()).arg(meta->className()).arg(id);
        logError() << _errorMessage;
        return false;
      }
    }
  }

  return true;
}


bool
AbstractConfigReader::parseExtensions(
    const QHash<QString, AbstractConfigReader *> &extensions, ConfigObject *obj,
    const YAML::Node &node, ConfigObject::Context &ctx)
{
  // Parse extensions:
  foreach (QString name, extensions.keys()) {
    if (node[name.toStdString()]) {
      YAML::Node extNode = node[name.toStdString()];
      ConfigObject *ext = extensions[name]->allocate(extNode, ctx);
      if (!ext) {
        _errorMessage = tr("%1:%2: Cannot allocate extension '%3': %1")
            .arg(extNode.Mark().line).arg(extNode.Mark().column)
            .arg(name).arg(extensions[name]->errorMessage());
        return false;
      }
      if (! extensions[name]->parse(ext, extNode, ctx)) {
        _errorMessage = tr("%1:%2: Cannot parse extension '%3': %1")
            .arg(extNode.Mark().line).arg(extNode.Mark().column)
            .arg(name).arg(extensions[name]->errorMessage());
        return false;
      }
      obj->addExtension(name, ext);
    }
  }

  return true;
}

bool
AbstractConfigReader::linkExtensions(
    const QHash<QString, AbstractConfigReader *> &extensions, ConfigObject *obj,
    const YAML::Node &node, const ConfigObject::Context &ctx)
{
  foreach (QString name, extensions.keys()) {
    // skip extensions not set
    if (! obj->hasExtension(name))
      continue;
    YAML::Node extNode = node[name.toStdString()];
    AbstractConfigReader *reader = extensions[name];
    ConfigObject *extension = obj->extension(name);
    if (!reader->link(extension, extNode, ctx)) {
      _errorMessage = tr("%1:%2: Cannot link configuration extension '%3': %4")
          .arg(extNode.Mark().line).arg(extNode.Mark().column)
          .arg(name).arg(extensions[name]->errorMessage());
      return false;
    }
  }
  return true;
}


/* ********************************************************************************************* *
 * Implementation of ExtensionReader
 * ********************************************************************************************* */
ExtensionReader::ExtensionReader(QObject *parent)
  : AbstractConfigReader(parent)
{
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of ConfigReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> ConfigReader::_extensions;

ConfigReader::ConfigReader(QObject *parent)
  : AbstractConfigReader(parent)
{
  // pass...
}

AbstractConfigReader *
ConfigReader::addExtension(ExtensionReader *reader) {
  QString name = reader->metaObject()->className();
  if (0 <= reader->metaObject()->indexOfClassInfo("name")) {
    name = reader->metaObject()->classInfo(reader->metaObject()->indexOfClassInfo("name")).value();
  }

  if (_extensions.contains(name))
    return nullptr;

  _extensions[name] = reader;
  return reader;
}

bool
ConfigReader::read(Config *obj, const QString &filename) {
  YAML::Node node;
  try {
     node = YAML::LoadFile(filename.toStdString());
  } catch (const YAML::Exception &err) {
    _errorMessage = tr("Cannot read YAML codeplug from file '%1': %2")
        .arg(filename).arg(QString::fromStdString(err.msg));
    return false;
  }

  if (! node) {
    _errorMessage = tr("Cannot read YAML codeplug from file '%1'")
        .arg(filename);
    return false;
  }

  obj->clear();
  ConfigObject::Context context;

  if (! parse(obj, node, context))
    return false;

  if (! link(obj, node, context))
    return false;

  return true;
}

ConfigObject *
ConfigReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node); Q_UNUSED(ctx);
  return new Config();
}

bool
ConfigReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx)
{
  Config *config = qobject_cast<Config *>(obj);
  if (nullptr == config) {
    _errorMessage = tr("%1:%2: Cannot read configuration: passed object is not of type 'Config'.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  if (! node.IsMap()) {
    _errorMessage = tr("%1:%2: Cannot read configuration, element is not a map.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  if (node["version"] && node["version"].IsScalar()) {
    ctx.setVersion(QString::fromStdString(node["version"].as<std::string>()));
    logDebug() << "Using format version " << ctx.version() << ".";
  } else {
    logWarn() << "No version string set, assuming 0.9.0.";
    ctx.setVersion("0.9.0");
  }

  if (! parseSettings(config, node["settings"], ctx))
    return false;

  if (! parseRadioIDs(config, node["radioIDs"], ctx))
    return false;

  if (! parseContacts(config, node["contacts"], ctx))
    return false;

  if (! parseGroupLists(config, node["groupLists"], ctx))
    return false;

  if (! parseChannels(config, node["channels"], ctx))
    return false;

  if (! parseZones(config, node["zones"], ctx))
    return false;

  if (! parseScanLists(config, node["scanLists"], ctx))
    return false;

  if (! parsePositioningSystems(config, node["positioning"], ctx))
    return false;

  if (! parseRoamingZones(config, node["roaming"], ctx))
    return false;

  if (! parseExtensions(_extensions, config, node, ctx))
    return false;

  return true;
}

bool
ConfigReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx)
{
  Config *config = qobject_cast<Config *>(obj);

  // radio IDs must be linked before settings, as they may refer to the default DMR ID
  if (! linkRadioIDs(config, node["radioIDs"], ctx))
    return false;

  if (! linkSettings(config, node["settings"], ctx))
    return false;

  if (! linkContacts(config, node["contacts"], ctx))
    return false;

  if (! linkGroupLists(config, node["groupLists"], ctx))
    return false;

  if (! linkChannels(config, node["channels"], ctx))
    return false;

  if (! linkZones(config, node["zones"], ctx))
    return false;

  if (! linkScanLists(config, node["scanLists"], ctx))
    return false;

  if (! linkPositioningSystems(config, node["positioning"], ctx))
    return false;

  if (! linkRoamingZones(config, node["roaming"], ctx))
    return false;

  // link extensions
  if (! linkExtensions(_extensions, config, node, ctx))
    return false;

  return true;
}


bool
ConfigReader::parseSettings(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  RadioSettingsReader reader;
  if (! reader.parse(config->settings(), node, ctx)) {
    _errorMessage = reader.errorMessage();
    return false;
  }
  return true;
}

bool
ConfigReader::linkSettings(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (node["defaultID"] && node["defaultID"].IsScalar()) {
    QString id = QString::fromStdString(node["defaultID"].as<std::string>());
    if (ctx.contains(id) && ctx.getObj(id)->is<RadioID>()) {
      RadioID *def = ctx.getObj(id)->as<RadioID>();
      config->radioIDs()->setDefaultId(config->radioIDs()->indexOf(def));
      logDebug() << "Set default radio ID to '" << def->name() << "'.";
    } else {
      _errorMessage = tr("%1:%2: Default radio ID '%3' does not refer to a radio ID.")
          .arg(node["defaultID"].Mark().line).arg(node["defaultID"].Mark().column)
          .arg(id);
      return false;
    }
  } else {
    logInfo() << "No default DMR radio ID specified, use first defined radio ID.";
  }
  return true;
}


bool
ConfigReader::parseRadioIDs(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (!node) {
    _errorMessage = tr("No radio IDs defined.");
    return false;
  }

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'radio-ids' is not a sequence.");
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parseRadioID(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parseRadioID(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if ((! node.IsMap()) || (1 != node.size())) {
    _errorMessage = tr("%1:%2: Radio ID must map with a single element specifying the type.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;

  }

  std::string type = node.begin()->first.as<std::string>();
  if ("dmr" == type) {
    if (! parseDMRRadioID(config, node[type], ctx))
      return false;
  } else {
    _errorMessage = tr("%1:%2: Cannot parse radio id: unknown type '%3'.")
        .arg(node.Mark().line).arg(node.Mark().column).arg(QString::fromStdString(type));
  }

  return true;
}

bool
ConfigReader::parseDMRRadioID(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  RadioIdReader reader;
  RadioID *id = qobject_cast<RadioID*>(reader.allocate(node, ctx));
  if (nullptr == id) {
    _errorMessage = tr("%1:%2: Cannot allocate radio-id: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }
  if (! reader.parse(id, node, ctx)) {
    _errorMessage = reader.errorMessage();
    return false;
  }
  config->radioIDs()->add(id);
  return true;
}

bool
ConfigReader::linkRadioIDs(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->radioIDs()->count()); it++, i++) {
    if (! linkRadioID(config->radioIDs()->getId(i), *it, ctx))
      return false;
  }

  // If there is at least one radio ID -> set first one as default.
  if (config->radioIDs()->count()) {
    config->radioIDs()->setDefaultId(0);
  }

  return true;
}

bool
ConfigReader::linkRadioID(RadioID *id, const YAML::Node &node, const ConfigObject::Context &ctx) {
  std::string type = node.begin()->first.as<std::string>();
  if ("dmr" == type) {
    if (! linkDMRRadioID(id, node[type], ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkDMRRadioID(RadioID *id, const YAML::Node &node, const ConfigObject::Context &ctx) {
  RadioIdReader reader;
  if (! reader.link(id, node, ctx)) {
    _errorMessage = reader.errorMessage();
    return false;
  }
  return true;
}


bool
ConfigReader::parseChannels(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! node) {
    _errorMessage = tr("No channels defined.");
    return false;
  }

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'channels' element is not a sequence.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parseChannel(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parseChannel(Config *config, const YAML::Node &node, ConfigObject::Context &ctx)  {
  if ((! node.IsMap()) || (1 != node.size())) {
    _errorMessage = tr("%1:%2: Expected map with a single element.")
        .arg(node.Mark().line, node.Mark().column);
    return false;
  }

  std::string type = node.begin()->first.as<std::string>();
  if ("analog" == type) {
    if (! parseAnalogChannel(config, node[type], ctx))
      return false;
  } else if ("digital" == type) {
    if (! parseDigitalChannel(config, node[type], ctx))
      return false;
  } else {
    _errorMessage = tr("%1:%2: Uknown channel type '%3'")
        .arg(node.Mark().line).arg(node.Mark().column).arg(QString::fromStdString(type));
    return false;
  }

  return true;
}

bool
ConfigReader::parseDigitalChannel(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  DigitalChannelReader reader;
  DigitalChannel *ch = reader.allocate(node, ctx)->as<DigitalChannel>();
  if (! ch) {
    _errorMessage = tr("%1:%2: Cannot allocate digital channel: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }
  if (! reader.parse(ch, node, ctx)) {
    _errorMessage = reader.errorMessage();
    return false;
  }
  config->channelList()->add(ch);
  return true;
}

bool
ConfigReader::parseAnalogChannel(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  AnalogChannelReader reader;
  AnalogChannel *ch = reader.allocate(node, ctx)->as<AnalogChannel>();
  if (! ch) {
    _errorMessage = tr("%1:%2: Cannot allocate analog channel: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }
  if (! reader.parse(ch, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse analog channel: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }
  config->channelList()->add(ch);
  return true;
}

bool
ConfigReader::linkChannels(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->channelList()->count()); it++, i++) {
    if (! linkChannel(config->channelList()->channel(i), *it, ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkChannel(Channel *channel, const YAML::Node &node, const ConfigObject::Context &ctx) {
  std::string type = node.begin()->first.as<std::string>();
  if ("analog" == type) {
    if (! linkAnalogChannel(channel->as<AnalogChannel>(), node[type], ctx))
      return false;
  } else if ("digital" == type) {
    if (! linkDigitalChannel(channel->as<DigitalChannel>(), node[type], ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkAnalogChannel(AnalogChannel *channel, const YAML::Node &node, const ConfigObject::Context &ctx) {
  AnalogChannelReader reader;
  if (! reader.link(channel, node, ctx)) {
    _errorMessage = reader.errorMessage();
    return false;
  }
  return true;
}

bool
ConfigReader::linkDigitalChannel(DigitalChannel *channel, const YAML::Node &node, const ConfigObject::Context &ctx) {
  DigitalChannelReader reader;
  if (! reader.link(channel, node, ctx)) {
    _errorMessage = reader.errorMessage();
    return false;
  }
  return true;
}


bool
ConfigReader::parseZones(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! node) {
    _errorMessage = tr("No zones defined.");
    return false;
  }

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'zones' element is not a sequence.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parseZone(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parseZone(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  ZoneReader reader;

  Zone *zone = reader.allocate(node, ctx)->as<Zone>();
  if (nullptr == zone) {
    _errorMessage = tr("%1:%2: Cannot allocate zone: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  if (! reader.parse(zone, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse zone: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  config->zones()->add(zone);

  return true;
}

bool
ConfigReader::linkZones(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->zones()->count()); it++, i++) {
    if (! linkZone(config->zones()->zone(i), *it, ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkZone(Zone *zone, const YAML::Node &node, const ConfigObject::Context &ctx) {
  ZoneReader reader;
  if(! reader.link(zone, node, ctx)) {
    _errorMessage = reader.errorMessage();
    return false;
  }
  return true;
}


bool
ConfigReader::parseScanLists(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! node)
    return true;

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'scan-lists' element is not a sequence.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parseScanList(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parseScanList(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  ScanListReader reader;

  ScanList *list = reader.allocate(node, ctx)->as<ScanList>();
  if (nullptr == list) {
    _errorMessage = tr("%1:%2: Cannot allocate scan list: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  if (! reader.parse(list, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse scan list: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  config->scanlists()->add(list);

  return true;
}

bool
ConfigReader::linkScanLists(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->scanlists()->count()); it++, i++) {
    if (! linkScanList(config->scanlists()->scanlist(i), *it, ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkScanList(ScanList *list, const YAML::Node &node, const ConfigObject::Context &ctx) {
  return ScanListReader().link(list, node, ctx);
}


bool
ConfigReader::parseContacts(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! node)
    return true;

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'contacts' element is not a sequence.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parseContact(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parseContact(Config *config, const YAML::Node &node, ConfigObject::Context &ctx)  {
  if ((! node.IsMap()) || (1 != node.size())) {
    _errorMessage = tr("%1:%2: Expected map with a single element.")
        .arg(node.Mark().line, node.Mark().column);
    return false;
  }

  std::string type = node.begin()->first.as<std::string>();
  if ("dmr" == type) {
    if (! parseDMRContact(config, node[type], ctx))
      return false;
  } else if ("dtmf" == type) {
    if (! parseDTMFContact(config, node[type], ctx))
      return false;
  } else {
    _errorMessage = tr("%1:%2: Uknown contact type '%3'")
        .arg(node.Mark().line).arg(node.Mark().column).arg(QString::fromStdString(type));
    return false;
  }

  return true;
}

bool
ConfigReader::parseDMRContact(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  DMRContactReader reader;
  DigitalContact *cont = reader.allocate(node, ctx)->as<DigitalContact>();
  if (nullptr == cont) {
    _errorMessage = tr("%1:%2: Cannot allocate private call contact: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  if (! reader.parse(cont, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse private call contact: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  config->contacts()->add(cont);

  return true;
}

bool
ConfigReader::parseDTMFContact(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  Q_UNUSED(config); Q_UNUSED(ctx)
  _errorMessage = tr("%1:%2: DTMF contact reader not implemented yet.")
      .arg(node.Mark().line).arg(node.Mark().column);
  return false;
}

bool
ConfigReader::linkContacts(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->contacts()->count()); it++, i++) {
    if (! linkContact(config->contacts()->contact(i), *it, ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkContact(Contact *contact, const YAML::Node &node, const ConfigObject::Context &ctx) {
  std::string type = node.begin()->first.as<std::string>();
  if ("dmr" == type) {
    if (! linkDMRContact(contact->as<DigitalContact>(), node[type], ctx))
      return false;
  } else if ("dtmf" == type) {
    if (! linkDTMFContact(contact->as<DTMFContact>(), node[type], ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkDMRContact(DigitalContact *contact, const YAML::Node &node, const ConfigObject::Context &ctx) {
  return DMRContactReader().link(contact, node, ctx);
}

bool
ConfigReader::linkDTMFContact(DTMFContact *contact, const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(contact)
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  _errorMessage = tr("%1:%2: DTMF contact not implemented yet.");
  return false;
}


bool
ConfigReader::parseGroupLists(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! node) {
    _errorMessage = tr("No group lists defined.");
    return false;
  }

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'group-lists' element is not a sequence.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parseGroupList(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parseGroupList(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  GroupListReader reader;

  RXGroupList *list = reader.allocate(node, ctx)->as<RXGroupList>();
  if (nullptr == list) {
    _errorMessage = tr("%1:%2: Cannot allocate group list: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  if (! reader.parse(list, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse group list: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  config->rxGroupLists()->add(list);

  return true;
}

bool
ConfigReader::linkGroupLists(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->rxGroupLists()->count()); it++, i++) {
    if (! linkGroupList(config->rxGroupLists()->list(i), *it, ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkGroupList(RXGroupList *list, const YAML::Node &node, const ConfigObject::Context &ctx) {
  return GroupListReader().link(list, node, ctx);
}


bool
ConfigReader::parsePositioningSystems(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! node)
    return true;

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'positioning' element is not a sequence.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parsePositioningSystem(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parsePositioningSystem(Config *config, const YAML::Node &node, ConfigObject::Context &ctx)  {
  if ((! node.IsMap()) || (1 != node.size())) {
    _errorMessage = tr("%1:%2: Expected map with a single element.")
        .arg(node.Mark().line, node.Mark().column);
    return false;
  }

  std::string type = node.begin()->first.as<std::string>();
  if ("dmr" == type) {
    if (! parseGPSSystem(config, node[type], ctx))
      return false;
  } else if ("aprs" == type) {
    if (! parseAPRSSystem(config, node[type], ctx))
      return false;
  } else {
    _errorMessage = tr("%1:%2: Uknown positioning system type '%3'")
        .arg(node.Mark().line).arg(node.Mark().column).arg(QString::fromStdString(type));
    return false;
  }

  return true;
}

bool
ConfigReader::parseGPSSystem(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  GPSSystemReader reader;

  GPSSystem *sys = reader.allocate(node, ctx)->as<GPSSystem>();
  if (nullptr == sys) {
    _errorMessage = tr("%1:%2: Cannot allocate GPS system: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  if (! reader.parse(sys, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse GPS system: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  config->posSystems()->add(sys);

  return true;
}

bool
ConfigReader::parseAPRSSystem(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  APRSSystemReader reader;

  APRSSystem *sys = reader.allocate(node, ctx)->as<APRSSystem>();
  if (nullptr == sys) {
    _errorMessage = tr("%1:%2: Cannot allocate APRS system: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  if (! reader.parse(sys, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse APRS system: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  config->posSystems()->add(sys);

  return true;
}

bool
ConfigReader::linkPositioningSystems(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->posSystems()->count()); it++, i++) {
    if (! linkPositioningSystem(config->posSystems()->system(i), *it, ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkPositioningSystem(PositioningSystem *system, const YAML::Node &node, const ConfigObject::Context &ctx) {
  std::string type = node.begin()->first.as<std::string>();
  if ("dmr" == type) {
    if (! linkGPSSystem(system->as<GPSSystem>(), node[type], ctx))
      return false;
  } else if ("aprs" == type) {
    if (! linkAPRSSystem(system->as<APRSSystem>(), node[type], ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkGPSSystem(GPSSystem *system, const YAML::Node &node, const ConfigObject::Context &ctx) {
  return GPSSystemReader().link(system, node, ctx);
}

bool
ConfigReader::linkAPRSSystem(APRSSystem *system, const YAML::Node &node, const ConfigObject::Context &ctx) {
  return APRSSystemReader().link(system, node, ctx);
}


bool
ConfigReader::parseRoamingZones(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! node)
    return true;

  if (! node.IsSequence()) {
    _errorMessage = tr("%1:%2: 'roaming' element is not a sequence.")
        .arg(node.Mark().line).arg(node.Mark().column);
    return false;
  }

  for (YAML::const_iterator it=node.begin(); it!=node.end(); it++) {
    if (! parseRoamingZone(config, *it, ctx))
      return false;
  }

  return true;
}

bool
ConfigReader::parseRoamingZone(Config *config, const YAML::Node &node, ConfigObject::Context &ctx) {
  RoamingReader reader;

  RoamingZone *zone = reader.allocate(node, ctx)->as<RoamingZone>();
  if (nullptr == zone) {
    _errorMessage = tr("%1:%2: Cannot allocate roaming zone: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  if (! reader.parse(zone, node, ctx)) {
    _errorMessage = tr("%1:%2: Cannot parse roaming zone: %3")
        .arg(node.Mark().line).arg(node.Mark().column).arg(reader.errorMessage());
    return false;
  }

  config->roaming()->add(zone);

  return true;
}

bool
ConfigReader::linkRoamingZones(Config *config, const YAML::Node &node, const ConfigObject::Context &ctx) {
  YAML::const_iterator it=node.begin();
  int i=0;
  for (; (it!=node.end()) && (i<config->roaming()->count()); it++, i++) {
    if (! linkRoamingZone(config->roaming()->zone(i), *it, ctx))
      return false;
  }
  return true;
}

bool
ConfigReader::linkRoamingZone(RoamingZone *zone, const YAML::Node &node, const ConfigObject::Context &ctx) {
  return RoamingReader().link(zone, node, ctx);
}


/* ********************************************************************************************* *
 * Implementation of ObjectReader
 * ********************************************************************************************* */
ObjectReader::ObjectReader(QObject *parent)
  : AbstractConfigReader(parent)
{
  // pass...
}

bool
ObjectReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx)
{
  if (! AbstractConfigReader::parse(obj, node, ctx))
    return false;

  if (! node.IsMap()) {
    _errorMessage = "Cannot parse object: Passed node is not a map.";
    return false;
  }

  if (node["id"] && node["id"].IsScalar()) {
    QString id = QString::fromStdString(node["id"].as<std::string>());
    if (ctx.contains(id)) {
      _errorMessage = tr("Cannot parse object '%1': ID already used.").arg(id);
      return false;
    }
    //logDebug() << "Register object " << obj << " as '" << id << "'.";
    ctx.add(id, obj);
  } else {
    logWarn() << "No ID associated with object, it cannot be referenced later.";
  }

  return true;
}


/* ********************************************************************************************* *
 * Implementation of RadioSettingsReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> RadioSettingsReader::_extensions;

RadioSettingsReader::RadioSettingsReader(QObject *parent)
  : AbstractConfigReader(parent)
{
  // pass...
}

bool
RadioSettingsReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
RadioSettingsReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new RadioSettings();
}

bool
RadioSettingsReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  RadioSettings *rid = qobject_cast<RadioSettings *>(obj);

  if (! AbstractConfigReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, rid, node, ctx))
    return false;

  return true;
}

bool
RadioSettingsReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of RadioIDReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> RadioIdReader::_extensions;

RadioIdReader::RadioIdReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

bool
RadioIdReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
RadioIdReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new RadioID("", 0);
}

bool
RadioIdReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  RadioID *rid = qobject_cast<RadioID *>(obj);

  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, rid, node, ctx))
    return false;

  return true;
}

bool
RadioIdReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of ChannelReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> ChannelReader::_extensions;

ChannelReader::ChannelReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

AbstractConfigReader *
ChannelReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return nullptr;
  _extensions[name] = ext;
  return ext;
}


bool
ChannelReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  Channel *channel = obj->as<Channel>();

  if ((!node["power"].IsDefined()) || ("!default" == node["power"].Tag())) {
    channel->setDefaultPower();
  } else if (node["power"].IsDefined() && node["power"].IsScalar()) {
    QMetaEnum meta = QMetaEnum::fromType<Channel::Power>();
    channel->setPower((Channel::Power)meta.keyToValue(node["power"].as<std::string>().c_str()));
  }

  if ((!node["timeout"].IsDefined()) || ("!default" == node["timeout"].Tag())) {
    channel->setDefaultTimeout();
  } else if (node["timeout"].IsDefined() && node["timeout"].IsScalar()) {
    channel->setTimeout(node["timeout"].as<unsigned>());
  }

  if ((!node["vox"].IsDefined()) || ("!default" == node["vox"].Tag())) {
    channel->setVOXDefault();
  } else if (node["vox"].IsDefined() && node["vox"].IsScalar()) {
    channel->setVOX(node["vox"].as<unsigned>());
  }

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
ChannelReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ObjectReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of DigitalChannelReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> DigitalChannelReader::_extensions;

DigitalChannelReader::DigitalChannelReader(QObject *parent)
  : ChannelReader(parent)
{
  // pass...
}

bool
DigitalChannelReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
DigitalChannelReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new DigitalChannel();
}

bool
DigitalChannelReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ChannelReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
DigitalChannelReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ChannelReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of AnalogChannelReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> AnalogChannelReader::_extensions;

AnalogChannelReader::AnalogChannelReader(QObject *parent)
  : ChannelReader(parent)
{
  // pass...
}

bool
AnalogChannelReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
AnalogChannelReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new AnalogChannel();
}

bool
AnalogChannelReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  AnalogChannel *channel = obj->as<AnalogChannel>();

  if (! ChannelReader::parse(obj, node, ctx))
    return false;

  channel->setRXTone(Signaling::SIGNALING_NONE);
  if (node["rxTone"] && node["rxTone"].IsMap()) {
    if (node["rxTone"]["ctcss"] && node["rxTone"]["ctcss"].IsScalar()) {
      channel->setRXTone(Signaling::fromCTCSSFrequency(node["rxTone"]["ctcss"].as<double>()));
    } else if (node["rxTone"]["dcs"] && node["rxTone"]["dcs"].IsScalar()) {
      int code = node["rxTone"]["dcs"].as<int>();
      bool inverted = (code < 0); code = std::abs(code);
      channel->setRXTone(Signaling::fromDCSNumber(code, inverted));
    }
  }

  channel->setTXTone(Signaling::SIGNALING_NONE);
  if (node["txTone"] && node["txTone"].IsMap()) {
    if (node["txTone"]["ctcss"] && node["txTone"]["ctcss"].IsScalar()) {
      channel->setTXTone(Signaling::fromCTCSSFrequency(node["txTone"]["ctcss"].as<double>()));
    } else if (node["txTone"]["dcs"] && node["txTone"]["dcs"].IsScalar()) {
      int code = node["txTone"]["dcs"].as<int>();
      bool inverted = (code < 0); code = std::abs(code);
      channel->setTXTone(Signaling::fromDCSNumber(code, inverted));
    }
  }

  if ((!node["squelch"].IsDefined()) || ("!default" == node["squelch"].Tag())) {
    channel->setSquelchDefault();
  } else if (node["squelch"].IsDefined() && node["squelch"].IsScalar()) {
    channel->setSquelch(node["squelch"].as<unsigned>());
  }

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
AnalogChannelReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ChannelReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of ZoneReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> ZoneReader::_extensions;

ZoneReader::ZoneReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

bool
ZoneReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
ZoneReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new Zone("");
}

bool
ZoneReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
ZoneReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ObjectReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of ContactReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> ContactReader::_extensions;

ContactReader::ContactReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

bool
ContactReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

bool
ContactReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
ContactReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of DigitalContactReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> DMRContactReader::_extensions;

DMRContactReader::DMRContactReader(QObject *parent)
  : ContactReader(parent)
{
  // pass...
}

AbstractConfigReader *
DMRContactReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return nullptr;
  _extensions[name] = ext;
  return ext;
}

ConfigObject *
DMRContactReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new DigitalContact(DigitalContact::PrivateCall, "", 0);
}

bool
DMRContactReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ContactReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
DMRContactReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ContactReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of PositioningReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> PositioningReader::_extensions;

PositioningReader::PositioningReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

bool
PositioningReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

bool
PositioningReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
PositioningReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ObjectReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of GPSSystemReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> GPSSystemReader::_extensions;

GPSSystemReader::GPSSystemReader(QObject *parent)
  : PositioningReader(parent)
{
  // pass...
}

bool
GPSSystemReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
GPSSystemReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new GPSSystem("");
}

bool
GPSSystemReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! PositioningReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
GPSSystemReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! PositioningReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of APRSSystemReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> APRSSystemReader::_extensions;

APRSSystemReader::APRSSystemReader(QObject *parent)
  : PositioningReader(parent)
{
  // pass...
}

bool
APRSSystemReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
APRSSystemReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new APRSSystem("",nullptr, "", 0, "", 0);
}

bool
APRSSystemReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  APRSSystem *system = qobject_cast<APRSSystem *>(obj);

  if (! PositioningReader::parse(obj, node, ctx))
    return false;

  if (node["source"] && node["source"].IsScalar()) {
    QString source = QString::fromStdString(node["source"].as<std::string>());
    QRegExp pattern("^([A-Z0-9]+)-(1?[0-9])$");
    if (! pattern.exactMatch(source)) {
      _errorMessage = tr("Cannot parse APRS system '%1': '%2' not valid source call and SSID.")
          .arg(system->name()).arg(source);
      return false;
    }
    system->setSource(pattern.cap(1), pattern.cap(2).toUInt());
  } else {
    _errorMessage = tr("Cannot parse APRS system '%1': No source specified.")
        .arg(system->name());
    return false;
  }

  if (node["destination"] && node["destination"].IsScalar()) {
    QString source = QString::fromStdString(node["destination"].as<std::string>());
    QRegExp pattern("^([A-Z0-9]+)-(1?[0-9])$");
    if (! pattern.exactMatch(source)) {
      _errorMessage = tr("Cannot parse APRS system '%1': '%2' not valid destination call and SSID.")
          .arg(system->name()).arg(source);
      return false;
    }
    system->setDestination(pattern.cap(1), pattern.cap(2).toUInt());
  } else {
    _errorMessage = tr("Cannot parse APRS system '%1': No destination specified.")
        .arg(system->name());
    return false;
  }

  if (node["path"] && node["path"].IsSequence()) {
    QStringList path;
    for (YAML::const_iterator it=node["path"].begin(); it!=node["path"].end(); it++) {
      if (it->IsScalar())
        path.append(QString::fromStdString(it->as<std::string>()));
    }
    system->setPath(path.join(""));
  }

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
APRSSystemReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! PositioningReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of ScanListReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> ScanListReader::_extensions;

ScanListReader::ScanListReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

bool
ScanListReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
ScanListReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new ScanList("");
}

bool
ScanListReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
ScanListReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ObjectReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}


/* ********************************************************************************************* *
 * Implementation of GroupListReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> GroupListReader::_extensions;

GroupListReader::GroupListReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

bool
GroupListReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
GroupListReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new RXGroupList("");
}

bool
GroupListReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
GroupListReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx)
{
  if (! ObjectReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}



/* ********************************************************************************************* *
 * Implementation of RoamingReader
 * ********************************************************************************************* */
QHash<QString, AbstractConfigReader *> RoamingReader::_extensions;

RoamingReader::RoamingReader(QObject *parent)
  : ObjectReader(parent)
{
  // pass...
}

bool
RoamingReader::addExtension(ExtensionReader *ext) {
  QString name = ext->metaObject()->className();
  if (0 <= ext->metaObject()->indexOfClassInfo("name")) {
    name = ext->metaObject()->classInfo(ext->metaObject()->indexOfClassInfo("name")).value();
  }
  if (_extensions.contains(name))
    return false;
  _extensions[name] = ext;
  return true;
}

ConfigObject *
RoamingReader::allocate(const YAML::Node &node, const ConfigObject::Context &ctx) {
  Q_UNUSED(node)
  Q_UNUSED(ctx)
  return new RoamingZone("");
}

bool
RoamingReader::parse(ConfigObject *obj, const YAML::Node &node, ConfigObject::Context &ctx) {
  if (! ObjectReader::parse(obj, node, ctx))
    return false;

  if (! parseExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}

bool
RoamingReader::link(ConfigObject *obj, const YAML::Node &node, const ConfigObject::Context &ctx) {
  if (! ObjectReader::link(obj, node, ctx))
    return false;

  if (! linkExtensions(_extensions, obj, node, ctx))
    return false;

  return true;
}
