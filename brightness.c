/**
 *
 * @mainpage Backlight
 *
 * @section Introduction
 * This program is a command line program that allows the brightness level of
 * an Intel video system to be read, set, incremented or decremented.
 * 
 * It will read and write @a /sys/class/backlight/intel_backlight/brightness
 * to establish the current backlight brightness and can write to that file to
 * change the brightness.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 * @section Usage 
 * backlight [OPTION...]
 * Option              | Description
 * ------------------- | --------------
 *       -d, --dec=INT | Decrement
 *       -i, --inc=INT | Increment
 *       -s, --set=INT | Set
 *       -v, --verbose | Produce verbose output
 *       -?, --help    | Give this help list
 *           --usage   | Give a short usage message
 *       -V, --version | Print program version
 * 
 * The program will report the maximum permitted brightness and the 
 * brightness setting when the program exits
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <argp.h>
#include <math.h>
#include <time.h>

#define BRIGHTNESS "/sys/class/backlight/intel_backlight/brightness"
#define MAX_BRIGHTNESS "/sys/class/backlight/intel_backlight/max_brightness"
/**
 * Stores the values of the program options that are passed
 * in from the command line. The values initially set to invalid values by
 * main and are updated as the command line parameters are parsed. It is 
 * only permissible to specify one of inc, dec, or set.
 */
typedef struct {
	int verbose;    /**< If set, be verbose */
	int quiet;      /**< If set, shush */
	int notify;     /**< If set, send a notification */
	int iconpath;   /**< If set, send a path to an icon */
	int percent;    /**< If set, interpret value as percentage */
	int tog;        /**< If set, toggle between off and on */
	int inc;        /**< Value by which to increment the brightness */
	int dec;        /**< Value by which to decrement the brightness */
	int set;        /**< Value by which to set the brightness */
} ProgramArguments;

static ProgramArguments arguments;

/// Define the acceptable command line options

static struct argp_option options[] =
{
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"quiet",   'q', 0, 0, "Produce no output"},
	{"notify",  'n', 0, 0, "Send notification"},
	{"iconpath",'I', 0, 0, "Output ONLY path to icon"},
	{"percent", 'p', 0, 0, "Interpret integer as percentage"},
	{"toggle",  't', 0, 0, "Toggle backlight"},
	{"inc", 'i', "INT",0,"Increment"},
	{"dec", 'd', "INT",0,"Decrement"},
	{"set", 's', "INT",0,"Set"},
	{0}
};

const char *argp_program_version = "backlight 0.2";
const char *argp_program_bug_address = "<ewwaller+code@gmail.com>";
static char doc[] = "backlight -- Read, set, increment, or decrement the "
                    "backlight on Intel graphics based displays";

static char args_doc[] = "";
static error_t parse_opt (int key, char *arg, struct argp_state *state);

static struct argp argp = { options, parse_opt, args_doc, doc };

/*
 * AFAIK the only way to make a fade effect is to repeatedly flush incremental
 * changes to the disk. I have not tested fade on a HDD but I would assume
 * it's much slower if not unusable, so use at your own risk (tested on a SSD)
 */

/*
 * change this to make the fading smoother or less resource intensive
 * calculation: no. of steps = 1/fade_step (0 = maximum steps)
 * 0 = smoothest, 0.5 = 2 steps
 */
static const double fade_step = 0.1;

/*
 * set fade speed in ms
 * total transition time will always be more than this due to IO slouchiness
 * 0 = disabled, 999 = why?
 */
static const int fade_time = 170;

/*
 * when brightness less than this, set as new brightness
 * be careful when testing this as you may have to reboot to regain sight
 * note: this is in native units not percentage so you may want to
 *       check what your headroom is
 * 0 = no lower limit (screen can turn off with dec and set)
 */
static const int lower_limit = 1;


int
ReadSysFile(char *theFileName)
{
	/** 
	 * Read a file from @a /sys and interpret the data on the first line of 
	 * the "file" as an integer expresed as a ascii decimal string 
	 * 
	 *  @param[in] *theFileName A zero terminated string containing the name
	 *                          and path of the sys file
	 * 
	 *  @return                 the integer value read from the file.
	 *                          -1 indicates failure. 
	 */

	char* readBuffer = NULL;
	long unsigned int bufferSize = 0;
	
	FILE *theFile = fopen(theFileName,"r");
	if (!theFile)
	{
		fprintf(stderr,"\nCould not open the file %s\n",theFileName);
		return -1;
	}
	
	getline(&readBuffer, &bufferSize, theFile);
	
	if (readBuffer)
	{
		int theIntValue = atoi(readBuffer);
		
		free(readBuffer);
		fclose(theFile);
		
		return (theIntValue);
	}
	fclose(theFile);
	return -1;
}

int
CheckPerm(char *theFileName)
{
	/**
	 * A function to check if this program can actually write to theFileName
	 *
	 * @param[in] *theFileName A pointer to a zero terminated string
	 *                         containing the name and path of the sys file
	 * 
	 * @return                 0 is success; negative integer is failure
	*/
	int current = ReadSysFile(theFileName);
	
	FILE *theFile = fopen(theFileName,"w");
	if (!theFile)
		return -1;
	int chars = fprintf(theFile,"%i\n",current);
	int rval  = fclose(theFile);
	//if all is good, return 0
	return (rval < 0 && chars > -1 ? rval
	                               : chars > 0 ? 0
	                                           : chars);
}

int
SetTo(char *theFileName, int target)
{
	/**
	 * Write a file from /sys an interpret the data on the first line of the
	 * "file" as an integer expresed as a ascii decimal string 
	 *
	 * @param[in] *theFileName A pointer to a zero terminated string
	 *                         containing the name and path of the sys file
	 * 
	 * @param[in] target       The value to be written to the file
	 * 
	 * @return                 0 or positive integer is success;
	 *                         negative integer is failure
	 */

	FILE *theFile = fopen(theFileName,"w");
	if (!theFile)
		return -1;

	int chars = fprintf(theFile,"%i\n",target);
	int rval  = fclose(theFile);
	
	return (rval < 0 && chars > -1 ? rval : chars);
}

int
FadeTo(char *theFileName, int current, int change)
{
	/**
	 * If fade_time and fade_step are in range, this function will transition
	 * brightness from current to target
	 *
	 * @param[in] *theFileName A pointer to a zero terminated string
	 *                         containing the name and path of the sys file
	 * 
	 * @param[in] current      The value to be transitioned from
	 * 
	 * @param[in] change       How much to transition in what direction
	 * 
	 * @return                 0 or positive integer is success
	 *                         negative integer is failure
	 */

	if(!change)
		return 0;

	int target = current+change;

	if(target < 1 || target > 852)
		return 0;

	if(fade_time < 1 || fade_time > 999
	|| fade_step < 0 || fade_step > 0.5)
	{
		//no beautiful fading to be done :(
		return SetTo(theFileName, target);
	}
	else
	{
		FILE *theFile = fopen(theFileName,"w");
		if (!theFile)
			return -1;
		/* return value and characters written */
		int rval, chars;
		
		/* so I don't have to call fflush() to write to disk */
		rval = setvbuf(theFile, NULL, _IONBF, BUFSIZ);
		if(rval)
			return -2;
		int step = (!fade_step ? (change < 0 ? -1 : 1)
		                       : (int)round(change*fade_step));
		
		/* 
		 * calculate time between iterations, proportional to 'change' + a bit
		 * extra, depending on IO speeds. Accounting for sleep inturruption in
		 * this case will only increase fade_time further, thus will be ignored
		 */
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = (unsigned long)fade_time*1000000L/((double)change/step);
		
		/* so im not checking this every iteration */
		if(change > 0)
		{
			for(;;)
			{
				current += step;
				if(current >= target)
				{
					chars = fprintf(theFile, "%i\n", target);
					rval = chars < 0 ? -2 : 0;
					break;
				}
				if((chars = fprintf(theFile, "%i\n", current) < 0))
				{
					rval = -2;
					break;
				}
				if((rval = nanosleep(&ts, NULL) && rval) && rval != EINTR)
				{
					rval = -3;
					break;
				}
			}
		}
		else
		{
			for(;;)
			{
				current += step;
				if(current <= target)
				{
					chars = fprintf(theFile, "%i\n", target);
					rval = chars < 0 ? -2 : 0;
					break;
				}
				if((chars = fprintf(theFile, "%i\n", current) < 0))
				{
					rval = -2;
					break;
				}
				if((rval = nanosleep(&ts, NULL) && rval) && rval != EINTR)
				{
					rval = -3;
					break;
				}
			}
		}
		if(rval > -1)
			rval = fclose(theFile);
		else
			fclose(theFile);
		return (rval < 0 && chars > -1 ? rval : chars);
	}
}

int
parseIntArgument(char *arg)
{
	/**
	 * Convert a null terminated string of decimal digits to an integer. Any
	 * non-decimal digits in the string will result in a failure.
	 * 
	 * @param[in] arg A pointer to null terminated string of decimal digits
	 * 
	 * @return    A positive or zero integer represented by the string.
	 * 
	 * @warning   An error condition will cause the program to exit with an
	 *            error.
	 */
	
	char *endptr, *str = NULL;
	long val;
	errno = 0; /* To distinguish success/failure after call */
	val = strtol(arg, &endptr, 10);
	
	if((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
	|| (errno != 0 && val == 0))
	{
		perror("strtol");
		exit(EXIT_FAILURE);
	}
	
	if (endptr == str)
	{
		fprintf(stderr, "No digits were found\n");
		exit(EXIT_FAILURE);
	}
	if (*endptr)
	{
		printf ("Non digit in decimal value\n");
		exit(EXIT_FAILURE);
	}
	/* If we got here, strtol() successfully parsed a number */
	return (int)val;
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
	/**
	 * Process the command line arguments and options. Collect all 
	 * the options and remember their state. This function is called one
	 * time for each key. As the keys are passed in, check to ensure they
	 * do not conflict with each other. When there are no more keys, 
	 * ensure there are no additional parameters that were passed into the 
	 * program -- this one does not want any.
	 *
	 * @param[in]     key    An integer that represents a char value of one 
	 *                       of the options passed from the command line.
	 * 
	 * @param[in,out] *arg   A pointer to a null terminated string that is the
	 *                       argument of the option represented by key.
	 * 
	 * @param[in]     *state f
	 */
	
	ProgramArguments* argumentPtr = state->input;
	
	switch (key)
	{
		/* testing argument mutual exclusivity is done by main */
		case 'v': argumentPtr->verbose  = 1; break;
		case 'q': argumentPtr->quiet    = 1; break;
		case 'n': argumentPtr->notify   = 1; break;
		case 'I': argumentPtr->iconpath = 1; break;
		case 'p': argumentPtr->percent  = 1; break;
		case 't': argumentPtr->tog      = 1; break;
		case 'i': arguments.inc=parseIntArgument(arg); break;
		case 'd': arguments.dec=parseIntArgument(arg); break;
		case 's': arguments.set=parseIntArgument(arg); break;
		
		case ARGP_KEY_NO_ARGS:
			/* If there are no Arguments, that is good.	We don't want any */
			break;
		case ARGP_KEY_ARG:
			/* I am not expecting any arguments that are not options. */
			argp_usage (state);
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

int
GetContainingPath(char **path)
{
	char buf[PATH_MAX];
	realpath("/proc/self/exe", buf);
	if (buf == NULL)
		return -1;
	const char *ptr = strstr(buf, "brightness");
	if (ptr == NULL)
		return -2;
	buf[ptr-buf] = '\0';
	*path = (char*) malloc(strlen(buf));
	strcpy(*path, buf);
	return sizeof(*path);
}


int
SetLock(int l_type) /* F_WRLCK or F_UNLCK */
{
	int fd;
	struct flock fl;
	fd = open("/tmp/brightLOCK", O_RDWR|O_CREAT);

	if(fd == -1)
		return -1;
	
	fl.l_type   = l_type;
	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len    = 0;
	fl.l_pid    = getpid();
	
	/* try to create a file lock */
	if(fcntl(fd, (l_type == F_WRLCK ? F_SETLKW : F_SETLK), &fl) == -1)
	{
		if(errno == EDEADLK)
			exit(EXIT_FAILURE); /* whoops */
		if(errno == EINTR || errno == EACCES || errno == EAGAIN)
			return -1;
	}
	if(l_type == F_UNLCK)
		close(fd);
	return 0;
}

int
main (int argc, char** argv)
{

	/**
	 * This is the main function.
	 * It will determine the maximum brightness permitted, and the current
	 * brightness, and will parse the parameters passed in on the command line
	 * and determine their validity. If they are valid, and they call for a
	 * change in the brightness setting, it will write to the appropriate
	 * system file to cause the brightness to change.
	 *
	 * @param[in]   argc    An integer that represents the number of command
	 *                      line parameters.
	 * @param[in]   **argv  A pointer to an array of pointers to null 
	 *                      terminated strings that store the parameters on
	 *                      the command line.
	 * @return              An integer that represents the exit value of the
	 *                      program. 0 means success.
	 */
	
	if(SetLock(F_WRLCK) == -1)
	{
		printf("Failed lock\n");
		return EXIT_FAILURE;
	}

	int max_brightness = ReadSysFile(MAX_BRIGHTNESS);
	if (max_brightness < 0)
		exit(EXIT_FAILURE);
	
	int brightness = ReadSysFile(BRIGHTNESS);
	if (brightness < 0 || brightness > max_brightness)
		exit(EXIT_FAILURE);

	/* booleans */
	arguments.verbose 	= 0;
	arguments.quiet 	= 0;
	arguments.notify 	= 0;
	arguments.iconpath 	= 0;
	arguments.percent 	= 0;
	arguments.tog 		= 0;

	/* ints */
	arguments.set = -1;
	arguments.inc = -1;
	arguments.dec = -1;
	
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	if(argc == 1)
	{
		printf("Max brightness = %i\n",max_brightness);
		printf("Current brightness = %i\n",brightness);
		return 0;
	}
	
	/* passive arguments do not directly modify the brightness */
	
	int totalPassive = arguments.verbose + arguments.notify
	                 + arguments.percent + arguments.iconpath
	                 + arguments.quiet;
	
	int totalNonPassive = (arguments.inc >= 0) + (arguments.dec >= 0)
	                    + (arguments.set >= 0) +  arguments.tog;

	if(arguments.verbose)
		printf("Arguments parsed = %i Passive, %i NonPassive\n",
		       totalPassive, totalNonPassive);
	
	if(arguments.iconpath)
		arguments.quiet = 1;
	
	/* check for invalid arguments */
	if (totalNonPassive > 1
	|| (arguments.verbose && (arguments.iconpath || arguments.quiet)))
	{
		if(arguments.verbose && arguments.iconpath)
			printf("Verbose and Iconpath conflict.\n");

		if(arguments.verbose && arguments.quiet)
			printf("Verbose and Quiet conflict.\n");

		if(totalNonPassive > 1)
			printf("Toggle, Increment, Decrement and Set "
			       "are mutually exclusive options.\n");

		printf("Exiting...\n");
		exit(EXIT_FAILURE);
	}
	
	/* quick check to see if we can't write to file but want to */
	int canwrite = !CheckPerm(BRIGHTNESS);
	if(!canwrite && !arguments.verbose && !arguments.iconpath)
	{
		printf("Unable to set brightness, check permissions. -v for more info."
		       " Exiting...\n");
		exit(EXIT_FAILURE);
	}
	
	char *path = NULL;
	int len = GetContainingPath(&path);
	
	if(path == NULL)
		return EXIT_FAILURE;
	
	char cachepath[len+20];
	sprintf(cachepath, "%sprev_brightness", path);

	const char *action;
	int change = 0, toggleoff = 0;
	
	/* for all my percentifying needs */
	double percentifier = 100/(double)max_brightness;
	
	if (arguments.tog)
	{
		if (brightness == 0)
		{
			change = ReadSysFile(cachepath);
			if(change < 1)
				change  = 1;
			action = "Toggled on, set to ";
		}
		else
		{
			if(SetTo(cachepath,brightness) > 0)
			{
				change = -brightness;
				toggleoff = 1;
				action = "Toggled off, saved previous brightness as ";
			}
			else if(!arguments.quiet)
				printf("Couldn't store current brightness, aborting toggle\n");
		}
	}
	else if (arguments.inc > 0)
	{
		change = (arguments.percent ? (int)round(arguments.inc / percentifier)
		                            : arguments.inc);
		action = "Incremented by ";
	}
	else if (arguments.dec > 0)
	{
		change = -(arguments.percent ? (int)round(arguments.dec / percentifier)
		                             : arguments.dec);
		action = "Decremented by ";
	}
	else if (arguments.set >= 0)
	{
		change = (arguments.percent
		       ? (int)round(arguments.set / percentifier) - brightness
		       : arguments.set - brightness);
		action = "Set to ";
	}
	
	int prev_brightness = brightness;
	brightness += change;
	
	/* make sure the change won't send brightness out of bounds */
	if(brightness < lower_limit)
		brightness = toggleoff? 0 : lower_limit;
	else if(brightness > max_brightness)
		brightness = max_brightness;
	
	change = brightness - prev_brightness;
	
	char *func = (char*) malloc(60);
	sprintf(func, "%s%i", action,
	        ((arguments.set > -1) ? brightness
	                              : abs(change)));
	if(arguments.percent)
		sprintf(func, "%s (%i%%)", func, 
		        (int)ceil(percentifier*((arguments.set > -1) ? brightness
		                                                     : abs(change))));
	
	/* calculate icon path and send notification if needed */
	if(arguments.verbose || arguments.notify || arguments.iconpath)
	{
		char iconpath[len+40];
		const char *icon = NULL;
		int x = (int)round(4.4*(double)brightness/max_brightness);
		switch(x)
		{
			case 0: icon = "off";	break;
			case 1: icon = "low";	break;
			case 2: icon = "medium";break;
			case 3: icon = "high";	break;
			case 4: icon = "full";	break;
			default: printf("wtf?\n");
		}
		sprintf(iconpath, "%snotification-display-brightness-%s.png",
		        path, icon);
		if(arguments.iconpath)
			printf(iconpath);
			
		/* only send notification if brightness will be changed */
		if (canwrite && change)
		{
			if(arguments.notify)
			{
				char notify[300] = {};
				sprintf(notify, "notify-send -t 500 -i %s -h int:transient:1 "
				                "-h int:value:%i -h string:synchronous:"
				                "brightness \"Brightness %s\"",
				        iconpath, (int)round(brightness*percentifier), func);
				system(notify);
			}
		}
		if (arguments.verbose)
		{
			printf("Path = %s\n", path);
			printf("Icon path = %s\n",iconpath);
		}
	}
	/* try to write new brightness */
	int chars = canwrite ? 0 : -1;
	if(canwrite && (change || arguments.verbose))
	{
		chars = FadeTo(BRIGHTNESS, prev_brightness, change);
	}
	
	SetLock(F_UNLCK);
	
	if(arguments.quiet)
	{
		free(func);
		free(path);
		return chars < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	/* print what happened... this part is much easier to read via terminal */
	if (arguments.verbose)
	{
		printf("Max brightness = %i\n",max_brightness);
		if(arguments.percent)
			printf("Current brightness = %i (%i%%)\n", brightness,
			      (int)ceil(percentifier * (chars > 0 ? brightness
			                                          : prev_brightness)));
		else
			printf("Current brightness = %i\n",
			       (chars > 0 ? brightness : prev_brightness));
		if(!chars)
		{
			if (arguments.set > max_brightness
			|| (arguments.percent && arguments.set > 100)
			||  arguments.inc > 0)
			{
				printf("Reached maximum brightness\n");
			}	
			else if(arguments.set <= 0
				 || arguments.dec > 0)
			{
				if(lower_limit > 0)
					printf("Reached minimum brightness, -t to turn off\n");
				else
					printf("Reached minimum brightness\n");
			}
			else
				printf("No change in brightness\n");
		}
		else if(chars == -1)
		{
			printf("Cannot write to %s\nMake sure %sbrightness is owned by "
			       "root.\nIf so, try \"sudo chmod u+s %sbrightness\"\n",
			       BRIGHTNESS, path, path);
		}
		else if(chars == -2)
		{
			printf("Nanosleep error\n");
		}
		else
		{
			printf("Characters written = %i\n", chars);
			printf("%s\n", func);
		}
	}
	else if(chars < 0)
	{
		printf("Unable to set brightness, -v for more info. Exiting...\n");
	}
	
	/* brightness was changed successfully */
	else if(chars > 0)
	{
		printf("Max brightness = %i\n",max_brightness);
		if(arguments.percent)
			printf("Current brightness = %i (%i%%)\n",
			       brightness, (int)ceil(percentifier * brightness));
		else
			printf("Current brightness = %i\n", brightness);
		printf("%s\n", func);
	}
	
	/* no change to brightness but it was requested */
	else if(totalNonPassive)
	{
			 if (arguments.set > max_brightness
			 || (arguments.percent && arguments.set > 100)
			 ||  arguments.inc > 0)
			printf("Reached maximum brightness: %i\n", max_brightness);
			
		else if(arguments.set <= 0
			 || arguments.dec > 0)
			printf("Reached minimum brightness, -t to turn off\n");
	}
	free(func);
	free(path);
	
	return chars < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
