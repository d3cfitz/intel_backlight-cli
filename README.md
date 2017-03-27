# intel_backlight-control
A simple C script to control an intel-based backlight

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
