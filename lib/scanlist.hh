#ifndef SCANLIST_HH
#define SCANLIST_HH

#include "configobject.hh"
#include <QAbstractListModel>

class Channel;
#include "configreference.hh"

/** Generic representation of a scan list.
 * @ingroup conf */
class ScanList : public ConfigObject
{
	Q_OBJECT

  /** The name of the scan list. */
  Q_PROPERTY(QString name READ name WRITE setName)
  /** The primary channel. */
  Q_PROPERTY(ChannelReference* primary READ primary)
  /** The secondary channel. */
  Q_PROPERTY(ChannelReference* secondary READ secondary)
  /** The revert channel. */
  Q_PROPERTY(ChannelReference* revert READ revert)
  /** The list of channels. */
  Q_PROPERTY(ChannelRefList * channels READ channels)

public:
  /** Constructs a scan list with the given name. */
	ScanList(const QString &name, QObject *parent=nullptr);

  /** Copies the given scan list. */
  ScanList &operator= (const ScanList &other);

  /** Returns the number of channels within the scanlist. */
	int count() const;
  /** Clears the scan list. */
	void clear();
  /** Returns the name of the scanlist. */
	const QString &name() const;
  /** Sets the name of the scanlist. */
	bool setName(const QString &name);

  /** Returns @c true if the given channel is part of this scanlist. */
  bool contains(Channel *channel) const;
  /** Returns the channel at the given index. */
	Channel *channel(int idx) const;
  /** Adds a channel to the scan list. */
  int addChannel(Channel *channel, int idx=-1);
  /** Removes the channel at the given index. */
	bool remChannel(int idx);
  /** Removes the given channel. */
	bool remChannel(Channel *channel);

  /** Returns the channels of the scan list. */
  const ChannelRefList *channels() const;
  /** Returns the channels of the scan list. */
  ChannelRefList *channels();

  /** Returns the primary channel reference. */
  const ChannelReference *primary() const;
  /** Returns the primary channel reference. */
  ChannelReference *primary();
  /** Returns the priority channel. */
  Channel *primaryChannel() const;
  /** Sets the priority channel. */
  void setPrimaryChannel(Channel *channel);

  /** Returns the secondary channel reference. */
  const ChannelReference *secondary() const;
  /** Returns the secondary channel reference. */
  ChannelReference *secondary();
  /** Returns the secondary priority channel. */
  Channel *secondaryChannel() const;
  /** Sets the secondary priority channel. */
  void setSecondaryChannel(Channel *channel);

  /** Returns the revert channel reference. */
  const ChannelReference *revert() const;
  /** Returns the revert channel reference. */
  ChannelReference *revert();
  /** Returns the TX channel. */
  Channel *revertChannel() const;
  /** Sets the TX channel. */
  void setRevertChannel(Channel *channel);

protected:
  /** The scanlist name. */
	QString _name;
  /** The channel list. */
  ChannelRefList _channels;
  /** The priority channel. */
  ChannelReference _primary;
  /** The secondary priority channel. */
  ChannelReference _secondary;
  /** The transmit channel. */
  ChannelReference _revert;
};


/** Represents the list of scan lists.
 * @ingroup conf */
class ScanLists: public ConfigObjectList
{
	Q_OBJECT

public:
  /** Constructs an empty list. */
	explicit ScanLists(QObject *parent = nullptr);

  /** Returns the scanlist at the given index. */
	ScanList *scanlist(int idx) const;

  int add(ConfigObject *obj, int row=-1);

	/** Implementation of QAbstractListModel, returns the number of scanlists. */
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
  /** Implementation of QAbstractListModel, returns the item data at the given index. */
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  /** Implementation of QAbstractListModel, returns the header data at the given section. */
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
};


#endif // SCANLIST_HH
