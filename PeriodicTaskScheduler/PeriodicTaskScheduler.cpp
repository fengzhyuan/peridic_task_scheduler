
#include "PeriodicTaskScheduler.h"
using namespace PeriodicTaskScheduler;

/*
	implementation of \class Thread
*/
Thread::Thread() : thread_(), stop_() {}
Thread::~Thread() noexcept {
	stop();
}

void Thread::start() {
	set_stop(false);
	set_pause(false);
	thread_ = thread(&Thread::run, this);
}

void Thread::stop() {
	set_stop(true);
	if (thread_.joinable()) { thread_.join(); }
}

void Thread::pause() { set_pause(true);  }

void Thread::resume() { set_pause(false); }

void Thread::worker_detach() { thread_.detach(); }

bool Thread::worker_joinable() { return thread_.joinable(); }

void Thread::worker_join() { thread_.join(); }

bool Thread::if_stop() { return stop_; }

bool Thread::if_pause() { return pause_; }

void Thread::guard_pause() {
	unique_lock<mutex> lock(mutex_);
	cv_wait_.wait(lock, [&] {return !if_pause();});
}

void Thread::set_stop(bool v) {
	stop_.store(v);
	if (v) { cv_wait_.notify_all(); }
}

void Thread::set_pause(bool v) {
	pause_.store(v);
	if (v) { cv_wait_.notify_all(); }
}

template<typename R, typename P>
void Thread::wait_for(duration<R, P> const & s) {
	unique_lock<mutex> lock(mutex_);
	cv_wait_.wait_for(lock, seconds(s), [&] {return if_stop() || if_pause();});
}

/*
	implementation of \class Task
*/

task_work_ptr Task::get_work() {
	return work_;
}

size_t Task::get_task_id() {
	return tid_;
}

size_t Task::get_period() {
	return period_;
}

void Task::run() {
	while (!if_stop()) { 
		while (if_pause());
		printf("working...task id:%zd, thread id:%ud, period:%zd \n", tid_, this_thread::get_id(), period_);
		
		auto start = steady_clock::now();
		float elapsed = work_(); // in million seconds
		// updata db if result is leagal
		if (elapsed >= .0) {
			db_->db_insert(tid_, tname_.c_str(), elapsed);
		}
		auto end = steady_clock::now();
		auto wait_time = seconds(period_) - duration_cast<seconds>(end - start);
		if (end - start > seconds::zero()) {
			wait_for(wait_time);
		}
	}
}

void Task::start() {
	printf("start task %zd\n", tid_);
	super::start();
}

void Task::stop() {
	super::stop();	
	printf("stop task %zd\n", tid_); /* before working function printing*/
}

void Task::pause() {
	super::pause();
	printf("pause task %zd\n", tid_);
}

void Task::resume() {
	super::resume();
	printf("resume task %zd\n", tid_);
}

void Task::update(size_t new_period) {
	printf("update task %zd period: [%zd]->[%zd]\n", tid_, period_, new_period);
	period_ = new_period;
}

Task::~Task() noexcept{
	super::stop();
	printf("destroy task %zd\n", tid_);
}
/*
	implementation of \class TaskScheduler
*/

bool TaskScheduler::setup_context() {

	bool status = true;
	try {
		db_ = db_handler_ptr(new SQLiteHandler("sqlite.db"));
		status &= db_->db_setup();
	}
	catch (...) {
		status = false;
	}
	return status;
}
size_t TaskScheduler::add_task(size_t period, task_work_ptr &work, string desc) {
	// check validity
	if (!period || !work) {
		return 0; 
	}
	lock_guard<mutex> lock(mu_dpool_);
	updated_ = false;
	size_t tid = ++task_counter;
	dyn_task_pool_.emplace_back(task_container_ptr(new Task(db_, period, tid, work, desc)));
	task_pool_.emplace(tid, dyn_task_pool_.back());
	updated_ = true;
	cv_dpool_.notify_all();

	return tid;
}

task_scheduler_ptr TaskScheduler::scheduler_ = nullptr;
void TaskScheduler::pause_task(size_t tid) {
	if (task_pool_.find(tid) == task_pool_.end()) {
		return;
	}
	task_pool_[tid]->pause();
}

void TaskScheduler::resume_task(size_t tid) {
	if (task_pool_.find(tid) == task_pool_.end()) {
		return;
	}
	task_pool_[tid]->resume();
}

bool TaskScheduler::update_task(size_t new_period, size_t tid) {
	if (task_pool_.find(tid) == task_pool_.end()) {
		return false;
	}
	try {
		auto task = task_pool_[tid];
		auto work = task->get_work();
		auto task_period = task->get_period();
		
		lock_guard<mutex> lock(mu_dpool_);
		pause_task(tid);
		updated_ = false;
		/*! deleted: replace with a new created thread for each update operation*/
		//dyn_task_pool_.emplace_back(task_container_ptr(new Task(new_period, tid, work)));
		//task_pool_[tid] = dyn_task_pool_.back();
		task->update(new_period);
		updated_ = true;
		resume_task(tid);
		cv_dpool_.notify_all();
	}
	catch (...) { return false; }
	return true;
}

void TaskScheduler::start() {
	super::start();
}

void TaskScheduler::run() {
	printf("Running TaskScheduler main thread...\n");
	
	while (!if_stop()) {
		unique_lock<mutex> locker(mu_dpool_);
		cv_dpool_.wait(locker, [this] { return updated_ == true; });
		if (!dyn_task_pool_.empty()) {
			for (auto &t : dyn_task_pool_) {
				t->start();
			}
			dyn_task_pool_.clear();
		}
	}

}

void TaskScheduler::cancel_task(size_t tid) {
	if (task_pool_.find(tid) == task_pool_.end()) {
		return;
	}
	task_pool_[tid]->stop();
	task_pool_.erase(tid);
}

void TaskScheduler::cancel_all() {
	vector<size_t> v_tids; 
	v_tids.reserve(task_pool_.size());

	for (auto &p : task_pool_) { v_tids.push_back(p.first); }
	for (auto tid : v_tids) { cancel_task(tid); }
}

void TaskScheduler::release_context() {
	cancel_all();	// cancel all task thread
	stop();			// cancel scheduler thread
}
