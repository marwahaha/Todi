/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QMimeData>
#include <QModelIndex>
#include <QPalette>
#include <QSet>
#include <QStringListModel>

#include "../lib/mpdparseutils.h"
#include "../lib/mpdstats.h"
#include "playlisttablemodel.h"

PlaylistTableModel::PlaylistTableModel(QObject *parent)
    : QAbstractTableModel(parent), song_id(-1) {
  connect(this, SIGNAL(modelReset()), this, SLOT(playListReset()));
}

PlaylistTableModel::~PlaylistTableModel() {
  // Update song list and refresh model
  while (this->songs.size()) delete this->songs.takeLast();
}

QVariant PlaylistTableModel::headerData(int section,
                                        Qt::Orientation orientation,
                                        int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
      case 0:
        return "Title";
      case 1:
        return "Artist";
      case 2:
        return "Album";
      case 3:
        return "Track";
      case 4:
        return "Time";
      case 5:
        return "Disc";
      default:
        break;
    }
  }

  return QVariant();
}

int PlaylistTableModel::rowCount(const QModelIndex &) const {
  return songs.size();
}

int PlaylistTableModel::columnCount(const QModelIndex &) const { return 6; }

QVariant PlaylistTableModel::data(const QModelIndex &index, int role) const {
  QPalette palette;

  if (!index.isValid()) return QVariant();

  if (index.row() >= songs.size()) return QVariant();

  // Mark background of song currently being played
  if (role == Qt::BackgroundRole && songs.at(index.row())->id == song_id) {
    return QVariant(palette.color(QPalette::Dark));
  }

  if (role == Qt::DisplayRole) {
    const Song *const song = songs.at(index.row());
    switch (index.column()) {
      case 0:
        return song->title;
      case 1:
        return song->artist;
      case 2:
        return song->album;
      case 3:
        if (song->track <= 0) return QVariant();
        return song->track;
      case 4:
        return Song::formattedTime(song->time);
      case 5:
        if (song->disc <= 0) return QVariant();
        return song->disc;
      default:
        break;
    }
  }

  return QVariant();
}

void PlaylistTableModel::updateCurrentSong(quint32 id) {
  qint32 oldIndex = -1;

  oldIndex = song_id;
  song_id = static_cast<qint32>(id);

  if (oldIndex != -1)
    emit dataChanged(index(songIdToRow(oldIndex), 0),
                     index(songIdToRow(oldIndex), 2));

  emit dataChanged(index(songIdToRow(song_id), 0),
                   index(songIdToRow(song_id), 2));
}

void PlaylistTableModel::updateSongs(QList<Song *> *songs) {
  // Update song list and refresh model
  while (this->songs.size()) delete this->songs.takeLast();

  this->songs = *songs;
  delete songs;
  beginResetModel();
  endResetModel();
}

qint32 PlaylistTableModel::getIdByRow(qint32 row) const {
  if (songs.size() <= row) {
    return -1;
  }

  return songs.at(row)->id;
}

qint32 PlaylistTableModel::getPosByRow(qint32 row) const {
  if (songs.size() <= row) {
    return -1;
  }

  return static_cast<qint32>(songs.at(row)->pos);
}

Qt::DropActions PlaylistTableModel::supportedDropActions() const {
  return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlaylistTableModel::flags(const QModelIndex &index) const {
  if (index.isValid())
    return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
  else
    return Qt::ItemIsDropEnabled;
}

/**
 * @return A QStringList with the mimetypes we support
 */
QStringList PlaylistTableModel::mimeTypes() const {
  QStringList types;
  types << "application/qtmpc_song_move_text";
  types << "application/qtmpc_songs_filename_text";
  return types;
}

/**
 * Convert the data at indexes into mimedata ready for transport
 *
 * @param indexes The indexes to pack into mimedata
 * @return The mimedata
 */
QMimeData *PlaylistTableModel::mimeData(const QModelIndexList &indexes) const {
  QMimeData *mimeData = new QMimeData();
  QByteArray encodedData;

  QDataStream stream(&encodedData, QIODevice::WriteOnly);

  /*
   * Loop over all our indexes. However we have rows*columns indexes
   * We pack per row so ingore the columns
   */
  QList<int> rows;
  foreach (QModelIndex index, indexes) {
    if (index.isValid()) {
      if (rows.contains(index.row())) continue;

      QString text = QString::number(getPosByRow(index.row()));
      stream << text;
      rows.append(index.row());
    }
  }

  mimeData->setData("application/qtmpc_song_move_text", encodedData);
  return mimeData;
}

/**
 * Act on mime data that is dropped in our model
 *
 * @param data The actual data that is dropped
 * @param action The action. This could mean drop/copy etc
 * @param row The row where it is dropper
 * @param column The column where it is dropper
 * @param parent The parent of where we have dropped it
 *
 * @return bool if we accest the drop
 */
bool PlaylistTableModel::dropMimeData(const QMimeData *data,
                                      Qt::DropAction action, int row,
                                      int /*column*/,
                                      const QModelIndex & /*parent*/) {
  if (action == Qt::IgnoreAction) {
    return true;
  }

  if (data->hasFormat("application/qtmpc_song_move_text")) {
    // Act on internal moves

    QByteArray encodedData = data->data("application/qtmpc_song_move_text");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    QList<quint32> items;

    int diff = row - lastClickedRow;
    if (diff == 0) {
      return true;
    }

    while (!stream.atEnd()) {
      QString text;
      stream >> text;

      // We only do real moves
      if (row < 0) {
        continue;
      }

      items.append(text.toUInt());
    }

    QList<quint32>::iterator bound;

    // Sort playlist to make moves easier
    if (diff > 0) {
      qSort(items.begin(), items.end(), qGreater<quint32>());
      diff -= 1;
    } else {
      qSort(items);
    }

    // Find if we have a playlist over or underflow
    for (QList<quint32>::iterator i = items.begin(); i != items.end(); i++) {
      if (diff > 0) {
        if (static_cast<qint32>(*i + static_cast<quint32>(diff)) >=
            songs.size() - 1) {
          bound = i + 1;
        }
      } else {
        if (static_cast<qint32>(*i + static_cast<quint32>(diff)) <= 0) {
          bound = i + 1;
        }
      }
    }

    if (diff > 0) {
      qSort(items.begin(), bound);
    } else {
      qSort(items.begin(), bound, qGreater<quint32>());
    }

    emit moveInPlaylist(items, static_cast<quint32>(diff), songs.size() - 1);
    return true;
  } else if (data->hasFormat("application/qtmpc_songs_filename_text")) {
    // Act on moves from the music library

    QByteArray encodedData =
        data->data("application/qtmpc_songs_filename_text");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    QStringList filenames;

    while (!stream.atEnd()) {
      QString text;
      stream >> text;

      // We only do real moves
      if (row < 0) {
        continue;
      }

      filenames << text;
    }

    // Check for empty playlist
    if (this->songs.size() == 1 && this->songs.at(0)->artist == "" &&
        this->songs.at(0)->album == "" && this->songs.at(0)->title == "") {
      emit filesAddedInPlaylist(filenames, 0, 0);
    } else {
      emit filesAddedInPlaylist(filenames, row, songs.size());
    }

    return true;
  }

  return false;
}

qint32 PlaylistTableModel::songIdToRow(qint32 id) const {
  for (int i = 0; i < songs.size(); i++) {
    if (songs.at(i)->id == id) {
      return i;
    }
  }

  return -1;
}

/**
 * Set the last clicked row so we can do pretty things
 * when dragging
 *
 * @param index The QModelIndex of the last click
 */

void PlaylistTableModel::clicked(const QModelIndex &index) {
  lastClickedRow = index.row();
}

/**
 * Act on a resetted playlist.
 *
 * --Update stats
 */
void PlaylistTableModel::playListReset() { playListStats(); }

/**
 * Set all the statistics to the stats singleton
 */
void PlaylistTableModel::playListStats() {
  MPDStats *const stats = MPDStats::getInstance();
  QSet<QString> artists;
  QSet<QString> albums;
  quint32 time = 0;

  // If playlist is empty return empty stats
  if (this->songs.size() == 1 && this->songs.at(0)->artist == "" &&
      this->songs.at(0)->album == "" && this->songs.at(0)->title == "") {
    stats->acquireWriteLock();
    stats->setPlaylistArtists(0);
    stats->setPlaylistAlbums(0);
    stats->setPlaylistSongs(0);
    stats->setPlaylistTime(0);
    stats->releaseWriteLock();
    emit playListStatsUpdated();
    return;
  }

  // Loop over all songs
  for (int i = 0; i < this->songs.size(); i++) {
    Song *current = this->songs.at(i);
    artists.insert(current->artist);
    albums.insert(current->album);
    time += current->time;
  }

  stats->acquireWriteLock();
  stats->setPlaylistArtists(static_cast<quint32>(artists.size()));
  stats->setPlaylistAlbums(static_cast<quint32>(albums.size()));
  stats->setPlaylistSongs(static_cast<quint32>(this->songs.size()));
  stats->setPlaylistTime(time);
  stats->releaseWriteLock();
  emit playListStatsUpdated();
}
