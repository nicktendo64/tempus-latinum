#include <pebble.h>
#include <ctype.h>
#include "num2words-en.h"

#define DEBUG 0
#define BUFFER_SIZE 44

Window *window;

typedef struct {
  TextLayer *currentLayer;
  TextLayer *nextLayer;
  PropertyAnimation *currentAnimation;
  PropertyAnimation *nextAnimation;
} Line;

Line line1;
Line line2;
Line line3;
TextLayer *date;
TextLayer *day;

static char line1Str[2][BUFFER_SIZE];
static char line2Str[2][BUFFER_SIZE];
static char line3Str[2][BUFFER_SIZE];

static void destroy_property_animation(PropertyAnimation **prop_animation) {
  if (*prop_animation == NULL) {
    return;
  }

  if (animation_is_scheduled((Animation*) *prop_animation)) {
    animation_unschedule((Animation*) *prop_animation);
  }

  property_animation_destroy(*prop_animation);
  *prop_animation = NULL;
}

//Handle Date
char* toRomanPlace(int place, const char* one_letter, const char* five_letter, const char* ten_letter)
{
	char* ret = (char*)malloc(5);
	*ret = '\0';
	
	if (place < 4)
	{
		for (int i=0; i<place; i++)
			strcat(ret, one_letter);
	}
	else if (place == 4)
	{
		strcat(ret, one_letter);
		strcat(ret, five_letter);
	}
	else if (place == 9)
	{
		strcat(ret, one_letter);
		strcat(ret, ten_letter);
	}
	else
	{
		strcat(ret, five_letter);
		
		for (int i=1; i<place-5; i++)
			strcat(ret, one_letter);
	}
	return ret;
}

char* toRomanNumeral(int num)
{
	int ones = num % 10;
	int tens = (num / 10) % 10;
	int hundreds = (num / 100) % 10;
	int thousands = (num / 1000) % 10;

	char* ret = (char*)malloc(15);
	*ret = '\0';
	
	for (int i=0; i<thousands; i++)
		strcat(ret, "M");
	
	strcat(ret, toRomanPlace(hundreds, "C", "D", "M"));
	strcat(ret, toRomanPlace(tens, "X", "L", "C"));
	strcat(ret, toRomanPlace(ones, "I", "V", "X"));
	
	return ret;
}

char* monthNumToRomanName(int month) {
	switch (month) {
		case 1:
			return "Januaris";
		case 2:
			return "Februaris";
		case 3:
			return "Martius";
		case 4:
			return "Aprilis";
		case 5:
			return "Maius";
		case 6:
			return "Iunius";
		case 7:
			return "Quintilis";
		case 8:
			return "Sextilis";
		case 9:
			return "September";
		case 10:
			return "October";
		case 11:
			return "November";
		case 12:
			return "December";
		default:
			APP_LOG(APP_LOG_LEVEL_WARNING, "Month not recognized: %i", month);
			return "";
	}
}


char* toRomanDate(int month, int day) {
	month++; // Bizarre storage scheme in struct tm
	
	char* ret = (char*) malloc(25);
	strcpy(ret, "");
	
	int longMonths[] = {1, 3, 5, 7, 8, 10, 12};
	bool longMonth = false;
	for (int i=0; i<7; i++)
	{
		if (longMonths[i] == month)
			longMonth = true;
	}
	
	int kalends = 1;
	
	int nones = longMonth ? 7  :  5;
	int ides  = longMonth ? 15 : 13;
	
	if (day == kalends)
	{
		strcat(ret, "Kal. ");
		strcat(ret, monthNumToRomanName(month));
	}
	else if (day == nones)
	{
		strcat(ret, "Non. ");
		strcat(ret, monthNumToRomanName(month));
	}
	else if (day == ides)
	{
		strcat(ret, "Ides ");
		strcat(ret, monthNumToRomanName(month));
	}
	else if (day < nones)
	{
		strcat(ret, "a.d.");
		strcat(ret, toRomanNumeral(nones - day + 1));
		strcat(ret, " Non. ");
		strcat(ret, monthNumToRomanName(month));
	}
	else if (day > nones && day < ides)
	{
		strcat(ret, "a.d.");
		strcat(ret, toRomanNumeral(nones - day + 1));
		strcat(ret, " Ides ");
		strcat(ret, monthNumToRomanName(month));
	}
	else
	{
		strcat(ret, "a.d.");
		strcat(ret, toRomanNumeral((longMonth ? 31 : 30) - day + 1));
		strcat(ret, " Kal. ");
		strcat(ret, monthNumToRomanName(month == 12 ? 1 : month + 1));
	}
	
	strcat(ret, ", ");
	return ret;
}

void setDate(struct tm *tm)
{
  static char* dateString;
  static char dayString[] = "wednesday";
  
  dateString = toRomanDate(tm->tm_mon, tm->tm_mday);
  
  strcat(dateString, toRomanNumeral((*tm).tm_year + 1900));
  
  strftime(dayString, sizeof(dayString), "%A", tm);
  if (strcmp(dayString, "Sunday") == 0)
	  strcpy(dayString, "dies Solis");
  else if (strcmp(dayString, "Monday") == 0)
	  strcpy(dayString, "dies Lunae");
  else if (strcmp(dayString, "Tuesday") == 0)
	  strcpy(dayString, "dies Martis");
  else if (strcmp(dayString, "Wednesday") == 0)
	  strcpy(dayString, "dies Mercurii");
  else if (strcmp(dayString, "Thursday") == 0)
	  strcpy(dayString, "dies Iovis");
  else if (strcmp(dayString, "Friday") == 0)
	  strcpy(dayString, "dies Veneris");
  else if (strcmp(dayString, "Saturday") == 0)
	  strcpy(dayString, "dies Saturnī");
	
  dateString[0] = tolower((int)dateString[0]);
  dayString[0] = tolower((int)dayString[0]);
  text_layer_set_text(date, dateString);
  text_layer_set_text(day, dayString);
}

// Animation handler
void animationStoppedHandler(struct Animation *animation, bool finished, void *context)
{
  Layer *current = (Layer *)context;
  GRect rect = layer_get_frame(current);
  rect.origin.x = 144;
  layer_set_frame(current, rect);
}

// Animate line
void makeAnimationsForLayers(Line *line, TextLayer *current, TextLayer *next)
{
  GRect rect = layer_get_frame((Layer*)next);
  rect.origin.x -= 144;

  destroy_property_animation(&(line->nextAnimation));

  line->nextAnimation = property_animation_create_layer_frame((Layer*)next, NULL, &rect);
  animation_set_duration((Animation*)line->nextAnimation, 400);
  animation_set_curve((Animation*)line->nextAnimation, AnimationCurveEaseOut);

  animation_schedule((Animation*)line->nextAnimation);

  GRect rect2 = layer_get_frame((Layer*)current);
  rect2.origin.x -= 144;

  destroy_property_animation(&(line->currentAnimation));

  line->currentAnimation = property_animation_create_layer_frame((Layer*)current, NULL, &rect2);
  animation_set_duration((Animation*)line->currentAnimation, 400);
  animation_set_curve((Animation*)line->currentAnimation, AnimationCurveEaseOut);

  animation_set_handlers((Animation*)line->currentAnimation, (AnimationHandlers) {
      .stopped = (AnimationStoppedHandler)animationStoppedHandler
      }, current);

  animation_schedule((Animation*)line->currentAnimation);
}

// Update line
void updateLineTo(Line *line, char lineStr[2][BUFFER_SIZE], char *value)
{
  TextLayer *next, *current;

  GRect rect = layer_get_frame((Layer*)line->currentLayer);
  current = (rect.origin.x == 0) ? line->currentLayer : line->nextLayer;
  next = (current == line->currentLayer) ? line->nextLayer : line->currentLayer;

  // Update correct text only
  if (current == line->currentLayer) {
    memset(lineStr[1], 0, BUFFER_SIZE);
    memcpy(lineStr[1], value, strlen(value));
    text_layer_set_text(next, lineStr[1]);
  } else {
    memset(lineStr[0], 0, BUFFER_SIZE);
    memcpy(lineStr[0], value, strlen(value));
    text_layer_set_text(next, lineStr[0]);
  }

  makeAnimationsForLayers(line, current, next);
}

// Check to see if the current line needs to be updated
bool needToUpdateLine(Line *line, char lineStr[2][BUFFER_SIZE], char *nextValue)
{
  char *currentStr;
  GRect rect = layer_get_frame((Layer*)line->currentLayer);
  currentStr = (rect.origin.x == 0) ? lineStr[0] : lineStr[1];

  if (memcmp(currentStr, nextValue, strlen(nextValue)) != 0 ||
      (strlen(nextValue) == 0 && strlen(currentStr) != 0)) {
    return true;
  }
  return false;
}

// Update screen based on new time
void display_time(struct tm *t)
{
  // The current time text will be stored in the following 3 strings
  char textLine1[BUFFER_SIZE];
  char textLine2[BUFFER_SIZE];
  char textLine3[BUFFER_SIZE];

  time_to_3words(t->tm_hour, t->tm_min, textLine1, textLine2, textLine3, BUFFER_SIZE);

  if (needToUpdateLine(&line1, line1Str, textLine1)) {
    updateLineTo(&line1, line1Str, textLine1);
  }
  if (needToUpdateLine(&line2, line2Str, textLine2)) {
    updateLineTo(&line2, line2Str, textLine2);
  }
  if (needToUpdateLine(&line3, line3Str, textLine3)) {
    updateLineTo(&line3, line3Str, textLine3);
  }
}

// Update screen without animation first time we start the watchface
void display_initial_time(struct tm *t)
{
  time_to_3words(t->tm_hour, t->tm_min, line1Str[0], line2Str[0], line3Str[0], BUFFER_SIZE);

  text_layer_set_text(line1.currentLayer, line1Str[0]);
  text_layer_set_text(line2.currentLayer, line2Str[0]);
  text_layer_set_text(line3.currentLayer, line3Str[0]);
  setDate(t);
}

// Configure the first line of text
void configureBoldLayer(TextLayer *textlayer)
{
  text_layer_set_font(textlayer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_GOTHAM_BOLD_39)));
  text_layer_set_text_color(textlayer, GColorWhite);
  text_layer_set_background_color(textlayer, GColorClear);
  text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Configure for the 2nd and 3rd lines
void configureLightLayer(TextLayer *textlayer)
{
  text_layer_set_font(textlayer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_GOTHAM_LIGHT_30)));
  text_layer_set_text_color(textlayer, GColorWhite);
  text_layer_set_background_color(textlayer, GColorClear);
  text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);

void init() {

  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);

  // 1st line layers
  line1.currentLayer = text_layer_create(GRect(0, 10, 144, 50));
  line1.nextLayer = text_layer_create(GRect(144, 10, 144, 50));
  configureBoldLayer(line1.currentLayer);
  configureBoldLayer(line1.nextLayer);

  // 2nd layers
  line2.currentLayer = text_layer_create(GRect(0, 47, 144, 50));
  line2.nextLayer = text_layer_create(GRect(144, 47, 144, 50));
  configureLightLayer(line2.currentLayer);
  configureLightLayer(line2.nextLayer);

  // 3rd layers
  line3.currentLayer = text_layer_create(GRect(0, 84, 144, 50));
  line3.nextLayer = text_layer_create(GRect(144, 84, 144, 50));
  configureLightLayer(line3.currentLayer);
  configureLightLayer(line3.nextLayer);

  #if INVERT
  //date & day layers
  date = text_layer_create(GRect(0, 150, 144, 168-150));
  text_layer_set_text_color(date, GColorBlack);
  text_layer_set_background_color(date, GColorClear);
  text_layer_set_font(date, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(date, GTextAlignmentRight);
  day = text_layer_create(GRect(0, 135, 144, 168-135));
  text_layer_set_text_color(day, GColorBlack);
  text_layer_set_background_color(day, GColorClear);
  text_layer_set_font(day, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(day, GTextAlignmentRight);
  #else
  //date & day layers
  date = text_layer_create(GRect(0, 150, 144, 168-150));
  text_layer_set_text_color(date, GColorWhite);
  text_layer_set_background_color(date, GColorClear);
  text_layer_set_font(date, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(date, GTextAlignmentRight);
  day = text_layer_create(GRect(0, 135, 144, 168-135));
  text_layer_set_text_color(day, GColorWhite);
  text_layer_set_background_color(day, GColorClear);
  text_layer_set_font(day, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(day, GTextAlignmentRight);
  #endif

  // Configure time on init
  time_t now = time(NULL);
  display_initial_time(localtime(&now));

  // Load layers
  Layer *window_layer = window_get_root_layer(window);
  layer_add_child(window_layer, (Layer*)line1.currentLayer);
  layer_add_child(window_layer, (Layer*)line1.nextLayer);
  layer_add_child(window_layer, (Layer*)line2.currentLayer);
  layer_add_child(window_layer, (Layer*)line2.nextLayer);
  layer_add_child(window_layer, (Layer*)line3.currentLayer);
  layer_add_child(window_layer, (Layer*)line3.nextLayer);
  layer_add_child(window_layer, (Layer*)date);
  layer_add_child(window_layer, (Layer*)day);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

void deinit() {

  tick_timer_service_unsubscribe();

  layer_destroy((Layer*)line1.currentLayer);
  layer_destroy((Layer*)line1.nextLayer);
  layer_destroy((Layer*)line2.currentLayer);
  layer_destroy((Layer*)line2.nextLayer);
  layer_destroy((Layer*)line3.currentLayer);
  layer_destroy((Layer*)line3.nextLayer);

  destroy_property_animation(&line1.currentAnimation);
  destroy_property_animation(&line1.nextAnimation);
  destroy_property_animation(&line2.currentAnimation);
  destroy_property_animation(&line2.nextAnimation);
  destroy_property_animation(&line3.currentAnimation);
  destroy_property_animation(&line3.nextAnimation);

  layer_destroy((Layer*)date);
  layer_destroy((Layer*)day);

  window_destroy(window);
}

// Time handler called every minute by the system
void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  display_time(tick_time);
  if (units_changed & DAY_UNIT) {
    setDate(tick_time);
  }
}

int main(void) {

  init();
  app_event_loop();
  deinit();

}
