#ifndef _DBHANDLER_H_ 
#define _DBHANDLER_H_ 
	
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <chrono>
#include <mutex>
#include "sqlite3.h"

class SQLiteHandler {
	sqlite3 *db_;
	const char *db_name_;
	bool is_open{ false };

	typedef unsigned long ulong;
	static unsigned long get_timestamp();

	std::mutex mutex_;
public:
	SQLiteHandler(const char *db_name):db_name_(db_name) {}
	~SQLiteHandler() noexcept;
	bool db_setup();

	bool db_insert(size_t tid, const char* task_name, float value);
	//int test();
};

#endif 
