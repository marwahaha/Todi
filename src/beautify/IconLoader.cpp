/* This file is part of Todi.

   Copyright 2016, Arun Narayanankutty <n.arun.lifescience@gmail.com>

   Todi is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   Todi is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Todi.  If not, see <http://www.gnu.org/licenses/>.

   Description : Todi icon loader
*/

#include "IconLoader.h"

#include <QFile>
#include <QtDebug>

int IconLoader::lumen_;
QList<QString> IconLoader::icon_path_;

void IconLoader::init() {
  icon_path_.clear();
  icon_path_ << ":icons/dark"
             << ":icons/light"
             << ":icons/common";
}

QIcon IconLoader::load(const QString& name, const IconMode& iconMode) {
  QIcon ret;
  // If the icon name is empty
  if (name.isEmpty()) {
    qWarning() << "Icon name is null";
    return ret;
  }

  QString filename;
  switch (iconMode) {
    case LightDark:
      (lumen_ < 100) ? filename = icon_path_.at(Dark)
                     : filename = icon_path_.at(Light);
      break;
    case General:
      filename = icon_path_.at(Indiscriminate);
      break;
  }

  const QString locate(filename + "/%1.%2");
  const QString filename_custom_png(locate.arg(name).arg("svg"));

  // First check if a png file is available
  if (QFile::exists(filename_custom_png)) {
    ret.addFile(filename_custom_png);
  }

  // if no icons are found, then...
  if (ret.isNull()) {
    qWarning() << "Couldn't load icon" << name;
  }
  return ret;
}
