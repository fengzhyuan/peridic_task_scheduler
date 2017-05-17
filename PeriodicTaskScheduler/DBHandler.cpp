#include "DBHandler.h"

#include <ios>
#include <iostream>

using namespace std;

#pragma warning(disable: 4996 )
#pragma warning(disable: 4244 )


bool SQLiteHandler::db_setup() {

	// set as thread safe
	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);

	
	// create table
	/*
	table `TASK` :
	id(primary key) | task id | task name | updte time | value | min value | max value | average value 
	===================================================================================================
	*/
	try {
		char *err;

		if (sqlite3_open(db_name_, &db_)) {
			throw exception("open db failed");
		}

		const char *cmd_create_tab = "CREATE TABLE IF NOT EXISTS TASK (ID INTEGER PRIMARY KEY, TID INTEGER, NAME STRING, TIME STRING, VALUE DOUBLE, MINVALUE DOUBLE, MAXVALUE DOUBLE, AVGVALUE DOUBLE)";

		if (sqlite3_exec(db_, cmd_create_tab, nullptr, nullptr, &err)) {
			printf(err);
			throw exception(err);
		}
		is_open = true;
	}
	catch (exception e) {
		printf(e.what());
		is_open = false;
	}
	return is_open;
}

bool SQLiteHandler::db_insert(size_t tid, const char *task_name, float value) {
	try {
		if (!is_open) return false;

		lock_guard<mutex> lock(mutex_);
		char cmd[1024];
		// get the most recent records of min/max/avg;  
		// use online average algorithm for simplicity
		// mean_n = mean_n-1 + (x_n - mean_n-1)/n
		sprintf(cmd, 
			"SELECT MINVALUE, MAXVALUE, AVGVALUE, TIME FROM TASK WHERE TID=%zd ORDER BY TIME DESC", 
			tid);
		int nrow = 0, ncol = 0;
		float val, minv, maxv, avgv;
		val = minv = maxv = avgv = value;

		ulong time = get_timestamp();
		char timestamp[16]; sprintf(timestamp, "%lu", time);
		char **tab = nullptr, *error = nullptr;

		if (sqlite3_get_table(db_, cmd, &tab, &nrow, &ncol, &error)) {
			throw exception("query table error");
		}
		if (nrow) {
			//for (int r = 0; r <= nrow; ++r) {
			//	for (int c = 0; c < ncol; ++c) {
			//		printf("%s\t", tab[r*ncol + c]);
			//	} printf("\n");
			//	if (!r) for (int c = 0; c < ncol; ++c) printf("----------"); printf("\n");
			//}
			int step = ncol;
			minv = atof(tab[step]), maxv = atof(tab[step + 1]), avgv = atof(tab[step + 2]);
			minv = min(minv, val), maxv = max(maxv, val), avgv = avgv + (val - avgv) / (float)(nrow+1.0);
		}
		sprintf(cmd, 
			"INSERT INTO TASK VALUES(NULL, %zd, '%s', '%s', %f, %f, %f, %f)", 
			tid, task_name, timestamp, val, minv, maxv, avgv);
		if (sqlite3_exec(db_, cmd, nullptr, nullptr, &error)) {
			throw exception(error);
			sqlite3_free(error);
		}
		printf("table updated: tid:%zd, tname:%s, val:%f, minv:%f, maxv:%f, avgv:%f\n", 
			tid, task_name, val, minv, maxv, avgv);
		sqlite3_free_table(tab);
	}
	catch (exception e) {
		printf("%s\n", e.what());
		return false;
	}
	return true;
}

unsigned long SQLiteHandler::get_timestamp() {
		return static_cast<unsigned long>
			(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::system_clock::now().time_since_epoch()).count());
}
SQLiteHandler::~SQLiteHandler() noexcept {
	if (is_open) sqlite3_close(db_);
}
