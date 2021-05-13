/* -*- coding: utf-8-unix -*-
 *
 * File: mobile/qml/TrackSpeedInfoBox.qml
 *
 * Copyright (C) 2021 Jukka Sirkka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.6

TrackInfoBox {

  id: info

  property real speed: NaN

  target: Rectangle {
    id: speedBox
    color: "white"
    radius: 4
    height: speed.height
    width: 1.5 * height + 2 * padding

    DimensionalValue {
      id: speed

      anchors {
        left: parent.left
        leftMargin: padding / 2
        rightMargin: padding / 2
        centerIn: parent
      }

      unit: "Kn"
      value: info.speed
      fontSize: info.fontSize
    }
  }
}