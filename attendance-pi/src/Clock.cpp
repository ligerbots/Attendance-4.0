#include "Clock.h"
#include "ANSI.h"

#include <stdio.h>
#include <thread>
#include <ctime>
#include <unistd.h>
#include <string>
#include <time.h>

namespace Clock {

	///Clock thead
	std::thread cThread;
	///Clock thread termination condition
	bool run = true;

	void init() {
		//Initialize the clock
		printf("[" WHITE "----" RESET "] Initializing Clock...");

		//Start clock thread
		cThread = std::thread(clockThread);

		//Success
		printf("\r[" GREEN "OKAY\n" RESET);
	}

	void destroy() {
		//Destroy the clock
		printf("[" WHITE "----" RESET "] Destroy Clock...");

		//Instruct the thread to terminate
		run = false;
		//Join the thread
		cThread.join();

		//Success
		printf("\r[" GREEN "OKAY\n" RESET);
	}

	void clockThread() {
		//Current time variable
		std::string date;

		//Loop
		while(run) {
			//Get formatted date
			date = getDate();
			//Debug
			printf("[" CYAN "INFO" RESET "] Current date is %s\n", date.c_str());
			//Delay
			sleep(1);
		}
	}

	std::string getDate() {
		//Time object
		std::time_t now = std::time(0);
		struct std::tm tstruct;
		char buf[16];
		//Calculate time
		tstruct = *std::localtime(&now);
		//Convert to string
		std::strftime(buf, sizeof(buf), "%m/%d      %I:%M", &tstruct);
		return buf;
	}
}
