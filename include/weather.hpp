
#define JSON_BUFF_DIMENSION 2500

unsigned long lastConnectionTime = 0;     // last time you connected to the server, in milliseconds
const unsigned long postInterval = 30 * 60 * 1000;  // posting interval of 30 minutes  (10L * 1000L; 10 seconds delay for testing)