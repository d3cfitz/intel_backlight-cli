# intel_backlight-control
Introduction

This program is a command line program that allows the brightness level of an Intel video system to be read, set, incremented or decremented. It will read and write @a /sys/class/backlight/intel_backlight/brightness to establish the current backlight brightness and can write to that file to change the brightness.

To establish the maximum allowed brightness, it will read /sys/class/backlight/intel_backlight/max_brightness

copyright 
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.

Usage

    path/to/backlight -[options...]

             Option | Description
     -d, --dec=INT  | Decrement
     -i, --inc=INT  | Increment
     -s, --set=INT  | Set
     -t, --toggle   | Toggle between off and on
     -v, --verbose  | Produce verbose output
     -q, --quiet    | No output
     -p, --percent  | Interpret input and output as percentages
     -I, --iconpath | Send only path to relevant icon
     -?, --help     | Give this help list
         --usage    | Give a short usage message
     -V, --version  | Print program version
 
The program will report the maximum permitted brightness and the 
brightness setting when the program exits

There are a few options where you can change how this program transitions from one brightness to the next if at all and the lower limit for the set and decrement options (so you don't accidentally turn your screen off)
note: toggle will always be able to turn the screen off
