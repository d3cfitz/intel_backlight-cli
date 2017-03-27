# intel_backlight-control
Comments from the original creator:
 * @mainpage Backlight
 *
 * @section Introduction
 * This program is a command line program that allows the brightness level of
 * an Intel video system to be read, set, incremented or decremented.
 * 
 * It will read and write @a /sys/class/backlight/intel_backlight/brightness
 * to establish the current backlight brightness and can write to that file to change 
 * the brightness.
 * 
 * To establish the maximum allowed brightness, it will read
 * @a /sys/class/backlight/intel_backlight/max_brightness
 * 
 * @author Eric Waller
 *
 * @date September 2015
 *
 * @copyright 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.	If not, see <http://www.gnu.org/licenses/>.
 * 
 * @section Usage 
 * backlight [OPTION...]
 * Option					| Description
 * --------------- | --------------
 *	 -d, --dec=INT | Decrement
 *	 -i, --inc=INT | Increment
 *	 -s, --set=INT | Set
 *	 -v, --verbose | Produce verbose output
 *	 -?, --help		| Give this help list
 *	     --usage	 | Give a short usage message
 *	 -V, --version | Print program version
 * 
 * The program will report the maximum permitted brightness and the 
 * brightness setting when the program exits
 
I've basically just taken his code and added a bunch of stuff as follows:

Commands:
-v --verbose
-p --iconpath
-n --notify
-t --toggle
-i --inc
-I --inc-perc
-d --dec
-D --dec-perc
-s --set
-S --set-perc

Most of these are self-explanatory but the general formula is -yx n where y is any number of 'passive' commands (-v, -p, -n), x is one of the modifying commands (the rest) and n is a number for said commands (except toggle, which just toggle your screen off/on).
Running the script without any arguments will just give you your current and maximum brightness

There are also options in the script that you can change:

fade_step = 30
	Change this to make the fading smoother or less resource intensive
	Warning: when making very rapid calls (i.e. faster than you can press), I don't recommend going lower than this
	0 = smoothest, 100 = none

fade_speed = 170
	Set fade speed in ms (within 0-1000)
	Long fade speeds tend to get caught up and interfere with each other, so i recommend not going over 300
	There is a flickering bug that occurs when decreasing brightness rapidly at intervals (e.g. 10% at a time). It is more noticeable as you increase the fade speed
	0 = disabled

lower_limit = 1
	When brightness is less than this, set as new brightness
	Be careful when testing this as you may have to reboot to regain sight
	Note: this is in native units not percentage so you may want to check what your headroom is
	0 = screen can turn off with dec, decperc, set and setperc
