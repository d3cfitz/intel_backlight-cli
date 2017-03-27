/**
 *
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
 *			 --usage	 | Give a short usage message
 *	 -V, --version | Print program version
 * 
 * The program will report the maximum permitted brightness and the 
 * brightness setting when the program exits
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <argp.h>
#include <math.h>
#include <time.h>

/**
 * Stores the values of the program options that are passed
 * in from the command line.	The values initially set to invalid values by
 * main and are updated as the command line parameters are parsed.	It is 
 * only permissible to specify one of inc, dec, or set.
 */
typedef struct {
	int verbose;	/**< If set, be verbose */
	int notify;		/**< If set, send a notification */
	int iconpath;	/**< If set, send a path to an icon */
	int tog;		/**< If set, toggle between off and on */
	int inc;		/**< Value by which to increment the brightness */
	int incperc;
	int dec;		/**< Value by which to decrement the brightness */
	int decperc;
	int set;		/**< Value by which to set the brightness */
	int setperc;
} ProgramArguments;

static ProgramArguments arguments;

///	Define the acceptable command line options

static struct argp_option options[] =							 
{
	{"verbose", 'v', 0, 0, "Produce verbose output"},	/**< The verbose command line flag has no argument */
	{"notify", 'n', 0, 0, "Send notification"},
	{"iconpath", 'p', 0, 0, "Output path to icon (for tint2)"},
	{"toggle", 't', 0, 0, "Toggle backlight"},
	{"inc", 'i', "INT",0,"Increment"},
	{"inc-perc", 'I', "INT",0,"Increment by percentage"},
	{"dec", 'd', "INT",0,"Decrement"},
	{"dec-perc", 'D', "INT",0,"Decrement by percentage"},
	{"set", 's', "INT",0,"Set"},
	{"set-perc", 'S', "INT",0,"Set to percentage"},
	{0}
};

const char *argp_program_version = "backlight 0.1";
const char *argp_program_bug_address = "<ewwaller+code@gmail.com>";
static char doc[] =
	"backlight -- Read, set, increment, or decrement the backlight on Intel graphics based displays";

static char args_doc[] = "";
static error_t parse_opt (int key, char *arg, struct argp_state *state);

static struct argp argp = { options, parse_opt, args_doc, doc };

//change this to make the fading smoother or less resource intensive
//0 = smoothest, 100 = none
static int fade_step = 20;

//set fade speed in ms (within 0-1000)
//0 = disabled
static int fade_speed = 170;

//when brightness less than this, set as new brightness
//be careful when testing this as you may have to reboot to regain sight
//note: this is in native units not percentage so you may want to check what your headroom is
//0 = screen can turn off with dec, decperc, set and setperc
static int lower_limit = 1;


int
ReadSysFile(char *theFileName)
{
	/**	Read a file from @a /sys and interpret the data on the first line of 
	 *	 the "file" as an integer expresed as a ascii decimal string 
	 *	 
	 *	 @param[in] *theFileName A zero terminated string containing the name and
	 *													 path of the sys file
	 *	 @return the integer value read from the file.	-1 indicates failure. 
	 */

	char* readBuffer = NULL;
	long unsigned int	bufferSize = 0;
	
	FILE *theFile = fopen(theFileName,"r");
	if (!theFile){
		fprintf(stderr,"\nCould not open the file %s\n",theFileName);
		return -1;
	}
	
	getline(&readBuffer, &bufferSize, theFile);
	if (readBuffer){
		int theIntValue=atoi(readBuffer);
		free(readBuffer);
		readBuffer=NULL;
		bufferSize=0;
		fclose(theFile);
		return (theIntValue);
	}
	fclose(theFile);
	return -1;
}

int
WriteSysFile(char *theFileName, int target)
{
	/**	
	 *	 Write a file from /sys an interpret the data on the first line of the "file" as an integer expresed as a ascii decimal string 
	 *		
	 *	 @param[in] *theFileName A pointer to a zero terminated string containing the 
	 *													 name and path of the sys file
	 *		@param[in] theValue		The value to be written to the file
	 *		@return								0 or positive integer is success;	negative integer is failure
	*/
	int returnValue = 1;
	//if writing a new brightness, smoothly transition to it step by step
	if (fade_speed && !strcmp("/sys/class/backlight/intel_backlight/brightness", theFileName)){
		int current = 0, change = 0, step = 1;
		current = ReadSysFile(theFileName);
		change = target-current;
		if(!change) return 0;
		else if(change<0) step = -1;
		else step = 1;
		if(fade_step > 0 && fade_step <= 100)
			step *= (int)ceil((double)fade_step/100*abs(change));
				printf("Fade: %i, %i\n",step, change);
		//have at least 2 iterations
		if(abs(change) <= abs(step)){
			FILE *theFile = fopen(theFileName,"w");
			if (!theFile) return -1;
			returnValue = fprintf(theFile,"%i\n",target);
			fclose(theFile);
			return returnValue;
		}
		
		struct timespec ts_rem;
		ts_rem.tv_sec = 0;
		ts_rem.tv_nsec = (unsigned long)fade_speed*1000000L/((double)change/step);
		printf("FadeWt: %.1f, %i, %i\n",fade_speed*1000000L/((double)change/step), step, change);
		while(returnValue > 0 && current != target){
			FILE *theFile = fopen(theFileName,"w");
			if (!theFile) return -1;
			current += step;
			if((change > 0 && current > target) || (change < 0 && current < target))
				current = target;
			if((returnValue = fprintf(theFile,"%i\n",current)) != -1){
				int result = 0;
				do{
					struct timespec ts_sleep = ts_rem;
					result = nanosleep(&ts_sleep, &ts_rem);
				}while(result == EINTR);
				if(result)
					returnValue = -2;
			}
			fclose(theFile);
		}
	}else{
		FILE *theFile = fopen(theFileName,"w");
		if (!theFile) return -1;
		returnValue = fprintf(theFile,"%i\n",target);
		fclose(theFile);
	}
	return returnValue;
}

int
parseIntArgument(char *arg)
{
	/**
	 *	Convert a null terminated string of decimal digits to an integer.	Any non-decimal 
	 *	digits in the string will result in a failure.
	 *	
	 *	@param[in] arg A pointer to null terminated string of decimal digits
	 *	@return A positive or zero integer represented by the string. 
	 *	@warning An error condition will cause the program to exit with an error.
	 */
	
	char *endptr, *str = NULL;
	long val;
	errno = 0;		/* To distinguish success/failure after call */
	val = strtol(arg, &endptr, 10);
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
		perror("strtol");
		exit(EXIT_FAILURE);
	}
	
	if (endptr == str) {
		fprintf(stderr, "No digits were found\n");
		exit(EXIT_FAILURE);
	}
	if (*endptr){
		printf ("Non digit in decimal value\n");
		exit(EXIT_FAILURE);
	}
	/* If we got here, strtol() successfully parsed a number */
	return (int)val;
}

void
TooManyOptions(void)
{
	/**
	 * A simple helper function that prints an error message and exits
	 * @warning Calling this function causes the program to exit with an error.
	 */

	printf("Toggle, Increment, Decrement and Set are mutually exclusive options\n");
	exit(EXIT_FAILURE);
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
	/**
	 *	Process the command line arguments and options.	Collect all 
	 *	the options and remember their state. This function is called one
	 *	time for each key.	As the keys are passed in, check to ensure they
	 *	do not conflict with each other.	When there are no more keys, 
	 *	ensure there are no additional parameters that were passed into the 
	 *	program -- this one does not want any.
	 *
	 *	@param[in] key		An integer that represents a char value of one 
	 *						of the options passed from the command line.
	 *	@param[in,out] *arg	A pointer to a null terminated string that is the argument of the 
	 *						option represented by key.
	 *	@param[in] *state	
	 */
	
	ProgramArguments* argumentPtr = state->input;
	
	switch (key)
		{
		case 'v':
			argumentPtr->verbose = 1;	break;
		case 'n':
			argumentPtr->notify = 1;	break;
		case 'p':
			argumentPtr->iconpath = 1;	break;
		case 't':
			if ((arguments.dec != -1) || (arguments.inc != -1) || (arguments.set != -1) || (arguments.decperc != -1) || (arguments.incperc != -1) || (arguments.setperc != -1))
				TooManyOptions();
			argumentPtr->tog = 1;	break;
		case 'i':
			if ((arguments.dec != -1) || (arguments.set != -1) || (arguments.tog != 0) || (arguments.decperc != -1) || (arguments.incperc != -1) || (arguments.setperc != -1))
				TooManyOptions();
			arguments.inc=parseIntArgument(arg);	break;
		case 'd':
			if ((arguments.inc != -1) || (arguments.set != -1) || (arguments.tog != 0) || (arguments.decperc != -1) || (arguments.incperc != -1) || (arguments.setperc != -1))
				TooManyOptions();
			arguments.dec=parseIntArgument(arg);	break;
		case 's':
			if ((arguments.dec != -1) || (arguments.inc != -1) || (arguments.tog != 0) || (arguments.decperc != -1) || (arguments.incperc != -1) || (arguments.setperc != -1))
				TooManyOptions();
			arguments.set=parseIntArgument(arg);	break;
		case 'I':
			if ((arguments.dec != -1) || (arguments.inc != -1) || (arguments.set != -1) || (arguments.tog != 0) || (arguments.decperc != -1) || (arguments.setperc != -1))
				TooManyOptions();
			arguments.incperc=parseIntArgument(arg);	break;
		case 'D':
			if ((arguments.dec != -1) || (arguments.inc != -1) || (arguments.set != -1) || (arguments.tog != 0) || (arguments.incperc != -1) || (arguments.setperc != -1))
				TooManyOptions();
			arguments.decperc=parseIntArgument(arg);	break;
		case 'S':
			if ((arguments.dec != -1) || (arguments.inc != -1) || (arguments.set != -1) || (arguments.tog != 0) || (arguments.decperc != -1) || (arguments.incperc != -1))
				TooManyOptions();
			arguments.setperc=parseIntArgument(arg);	break;
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

int main (int argc, char** argv)
{

	/**
	 * This is the main function.	 
	 * line. It will determine the maximum brightness permitted, and the current brightness,
	 * and will parse the parameters passed in on the command and determine their validity.	If
	 * they are valid, and they call for a change in the brightness setting, it will write to the
	 * appropriate system file to cause the brightness to change.	
	 *
	 *	@param[in]	argc	An integer that represents the number of command line parameters.
	 *	@param[in]	**argv	A pointer to an array of pointers to null terminated strings that store
	 *						the parameters on the command line.
	 *	@return				An integer that represents the exit value of the program.	0 means success.
	 */
	
	arguments.verbose = 0;
	arguments.notify = 0;
	arguments.iconpath = 0;
	arguments.tog = 0;
	arguments.set = -1;
	arguments.inc = -1;
	arguments.dec = -1;
	arguments.incperc = -1;
	arguments.decperc = -1;
	arguments.setperc = -1;

	int max_brightness = ReadSysFile("/sys/class/backlight/intel_backlight/max_brightness");
	if (max_brightness < 0)
		exit(EXIT_FAILURE);
	
	int brightness = ReadSysFile("/sys/class/backlight/intel_backlight/brightness");
	if (brightness < 0)
		exit(EXIT_FAILURE);
			
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	if(argc == 1){
		printf("Max Brightness = %i\n",max_brightness);
		printf("Current Brightness = %i\n",brightness);
		exit(EXIT_SUCCESS);
	}
	int canwrite = -1 != WriteSysFile("/sys/class/backlight/intel_backlight/brightness",brightness) ? 1 : 0;

	//get own file path
	int len;
	char *path = NULL;
	{
		char buf[PATH_MAX];
		realpath("/proc/self/exe", buf);
		if (buf == NULL){
			printf("Realpath failed, aborting...");
			exit(EXIT_FAILURE);
		}
		char *ptr = strstr(buf, "brightness");
		if (ptr == NULL)
			exit(EXIT_FAILURE);
		buf[ptr-buf] = '\0';
		len = strlen(buf);
		path = (char*) malloc(len);
		strcpy(path, buf);
	}
	len = sizeof(path);
		
	char cachepath[len+20];
	sprintf(cachepath, "%sprev_brightness", path);

	/*
	 * This code is ugly so I'll explain it...
	 * 1. change brightness value, making sure it doesn't end up out of range
	 * 2. store whatever was done to brightness in func (for verbose, notify or debugging)
	 * 3. set change to the difference
	 * 
	 * NOTE: Since when testing this I frequently turned my screen off by mistake and had to reboot,
	 * I've made it so that you cannot set the brightness lower than 1 unless you use --toggle
	 * this is entirely personal preference so change it if you want (option is up top)
	 */
	
	char func[60] = {};
	int prev_brightness = brightness, change = 0;
	if (arguments.tog)
	{
		if (brightness == 0){
			brightness = ReadSysFile(cachepath);
			change = brightness;
			sprintf(func, "Toggled on, set to %i (%.1f%%)\n",
			brightness,100*(double)brightness/max_brightness);
		}else{
			if(WriteSysFile(cachepath,prev_brightness) > 0){
				brightness = 0;
				change = prev_brightness;
				sprintf(func, "Toggled off, saved previous brightness as %i (%.1f%%)\n",
				prev_brightness, 100*(double)prev_brightness/max_brightness);
			}else
				printf("Couldn't store current brightness, aborting toggle\n");
		}
	}
	else if (arguments.inc > 0)
	{
		brightness += arguments.inc;
		brightness = brightness > max_brightness ? max_brightness : brightness;
		change = abs(brightness - prev_brightness);
		sprintf(func, "Incremented by %i\n", change);
	}
	else if (arguments.dec > 0)
	{
		brightness -= arguments.dec;
		brightness = brightness < 0 ? 0 : brightness;
		if(brightness < lower_limit) brightness = lower_limit;
		change = abs(brightness - prev_brightness);
		sprintf(func, "Decremented by %i\n", change); 
	}
	else if (arguments.set >= 0)
	{
		brightness = arguments.set > max_brightness ? max_brightness : arguments.set;
		if(brightness < lower_limit) brightness = lower_limit;
		change = abs(brightness - prev_brightness);
		sprintf(func, "Set to %i\n", brightness);
	}
	else if (arguments.incperc > 0)
	{
		brightness += (int)ceil(((double)arguments.incperc/100.0)*max_brightness);
		brightness = brightness > max_brightness ? max_brightness : brightness;
		change = abs(brightness - prev_brightness);
		sprintf(func, "Incremented by %.1f%%\n", 100.0*(double)change/max_brightness);
	}
	else if (arguments.decperc > 0)
	{
		brightness -= (int)ceil(((double)arguments.decperc/100)*max_brightness);
		brightness = brightness < 0 ? 0 : brightness;
		if(brightness < lower_limit) brightness = lower_limit;
		change = abs(brightness - prev_brightness);
		sprintf(func, "Decremented by %.1f%%\n", 100*(double)change/max_brightness);
	}
	else if (arguments.setperc >= 0)
	{
		brightness = arguments.setperc > 100 ? max_brightness : (int)round(((double)arguments.setperc/100)*max_brightness);
		if(brightness < lower_limit) brightness = lower_limit;
		change = abs(brightness - prev_brightness);
		sprintf(func, "Set to %.1f%%\n", 100.0*(double)brightness/max_brightness);
	}

	//calculate icon path and send notification if needed
	if(arguments.verbose || arguments.notify || arguments.iconpath){
		char iconpath[len+50];
		char icon[6] = {};
		int x = (int)round(4.4*(double)brightness/(double)max_brightness);
		switch(x){
			case 0: strcpy(icon, "off");	break;
			case 1: strcpy(icon, "low");	break;
			case 2: strcpy(icon, "medium");	break;
			case 3: strcpy(icon, "high");	break;
			case 4: strcpy(icon, "full");	break;
			default: printf("rounding error: %i", x);
		}
		//for tint2 executor
		sprintf(iconpath, "%snotification-display-brightness-%s.png", path, icon);
		if (arguments.iconpath) printf("%s\n",iconpath);
		
		if (canwrite && change && arguments.notify){
			char notify[300] = {};
			sprintf(notify, "notify-send -t 1000 -i %s -h int:transient:1 -h int:value:%i -h string:synchronous:brightness \"Brightness %s\"",
			iconpath, (int)floor(100*(double)brightness/(double)max_brightness), func);
			system(notify);
		}
		if (arguments.verbose){
			printf("Path = %s\n", path);
			if(!arguments.iconpath) printf("Icon path = %s\n",iconpath);
		}
	}
	//try to write new brightness
	int chars = canwrite ? 0 : -1;
	if(canwrite && (change || arguments.verbose))
		chars = WriteSysFile("/sys/class/backlight/intel_backlight/brightness",brightness);
	
	//print what happened
	if (arguments.verbose){
		printf("Max brightness = %i\n",max_brightness);
		printf("Current brightness = %i\n", chars > 0 ? brightness : prev_brightness);
		if(!chars){
			printf("No change in brightness\n");
		}else if(chars == -1){
			printf("Cannot write to \"/sys/class/backlight/intel_backlight/brightness\"\n");
			printf("Make sure %sbrightness is owned by root.\nIf so, try running \"sudo chmod u+s %sbrightness\"\n", path, path);
		}else if(chars == -2){
			printf("Nanosleep error\n");
		}else{
			printf("Characters written = %i\n", chars);
			printf("%s\n", func);
		}
	}else if(chars < 0){
		printf("Unable to set brightness, -v for more info. Exiting...\n");
	}else if(chars > 0 && !arguments.iconpath){
		printf("Max brightness = %i\n",max_brightness);
		printf("Current brightness = %i\n", brightness);
		printf("%s\n", func);
	}else if(!arguments.iconpath){
		//notify brightness cannot be inc/dec further
			 if(arguments.set > max_brightness
			 || arguments.setperc > max_brightness
			 || arguments.inc > 0
			 || arguments.incperc > 0)
			printf("Reached maximum brightness (%i)\n", max_brightness);
		else if(arguments.set <= 0
			 || arguments.setperc <= 0
			 || arguments.dec > 0
			 || arguments.decperc > 0)
			printf("Reached minimum brightness, -t to turn off\n");
	}
	free(path);
	exit(EXIT_SUCCESS);
}
