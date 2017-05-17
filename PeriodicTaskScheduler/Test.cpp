
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "PeriodicTaskScheduler.h"
#include "DBHandler.h"
#include "works.h"
using namespace std;
using namespace PeriodicTaskScheduler;

float work_1() {
	return tcp_connect("www.google.com", "80");
}

float work_2() {
	return icmp_ping("www.google.com"); 
}

float work_3() {
	return icmp_ping("www.stackoverflow.com");
}

int main(int argc, char **argv) {
	setvbuf(stdout, nullptr, _IONBF, 0);

	srand(time(nullptr));
	auto scheduler = TaskScheduler::get();
	// prepare resources
	if (!scheduler || !scheduler->setup_context()) {
		return -1;
	}
	task_work_ptr work1 = work_1, work2 = work_2, work3 = work_3;

	// start task1 and task2 at the beginning
	auto tid1 = scheduler->add_task(1, work1, "tcp google");
	auto tid2 = scheduler->add_task(2, work2, "ping google");

	vector<size_t> works{ tid1, tid2 };

	scheduler->start();

	size_t counter = 0;
	while (true) {
		if (counter == 20) { // end scheduler
			break;
		}
		if (counter == 7) { // cancel task 1
			scheduler->cancel_task(tid1);
			works.erase(find(works.begin(), works.end(), tid1));
		}
		if (counter == 11) {// add task 3
			works.push_back(scheduler->add_task(1, work3, "ping SO"));
		}
		if (counter && (counter % 5) == 0 && !works.empty()) { // update period
			scheduler->update_task(rand()%3+1, works[rand()%works.size()]);
		}
		++counter; this_thread::sleep_for(1s);
	}
	// release resources
	scheduler->release_context();
}