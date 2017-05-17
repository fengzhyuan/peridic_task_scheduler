
#ifndef _PERIODIC_TASK_SCHEDULER_H_
#define _PERIODIC_TASK_SCHEDULER_H_

#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>
#include "DBHandler.h"

using namespace std;
using namespace chrono;

namespace PeriodicTaskScheduler {
	class Task;
	class TaskScheduler;
	using task_work_ptr = function<float(void)>;		/* task function pointer */
	using task_container_ptr = shared_ptr<Task>;		/* encapsulated task pointer, used in 
														two different pools for lookup/update */
	using task_scheduler_ptr = shared_ptr<TaskScheduler>;	/* used for Task class */
	using db_handler_ptr = shared_ptr<SQLiteHandler>;	/* shared db handler with all threads */
	/**
		\description abstract class for task multi-threading
		containing a thread for each task instance
	*/
	class Thread {
	private:
		thread thread_;
		atomic<bool> stop_;					/* used for `wait_for` and `stop` thread */
		condition_variable cv_wait_;		/* used for `wait_for` and `stop` thread */
		mutex mutex_;						/* used for `wait_for` and `stop` thread */
		atomic<bool> pause_;
		
	protected:
		/**
			return status of `stop_`
		*/
		bool if_stop();
		bool if_pause();
		/**
			set `stop_` status; used for interuption in `wait_for` or thread stop
		*/
		void set_stop(bool v);
		void set_pause(bool v);
		void guard_pause();
		virtual void run() = 0;
	public:
		Thread();
		virtual ~Thread() noexcept;

		virtual void start();
		virtual void stop();
		virtual void pause();
		virtual void resume();
		/**
			let thread to wait for `s` seconds instead of using thread::wait_for so that 
			thread will be stopped/destroyed whenever `stop_` set to true

			@param s		wait time
		*/
		template<typename R, typename P>
		void wait_for(duration<R, P> const& s);
		
		/** util functions*/
		void worker_join();
		bool worker_joinable();
		void worker_detach();

		/** stop_ is not copyable */
		Thread(const Thread&)  = delete;
		Thread(const Thread&&) = delete;
		Thread & operator=(const Thread&)  = delete;
		Thread & operator=(const Thread&&) = delete;
	};

	/**
		\description class for each specified task; each task instance is `runnable` as a thread; 
		when updating a task, all of its resources except thread is unchanged, the following 
		methods will be called: pause()/update()/resume() 
		when canceling a task, stop() will be called and resource will be released thereafter by 
		TaskScheduler
	*/
	class TaskScheduler;
	class Task : public Thread {
		size_t period_;					/* task period, in seconds */
		size_t tid_;					/* identifier */
		task_work_ptr work_;			/* working function pointer */
		string tname_;					/* name/description of task */

		db_handler_ptr db_;				/* pointer to db instance */
		// make task instance non-copyable / non-movable
		Task(const Task&) = delete;
		Task(const Task&&) = delete;
		Task & operator = (const Task &) = delete;
		Task & operator = (const Task&&) = delete;
		
		using super = Thread;
	public:
		Task(db_handler_ptr db, size_t period, size_t id, const task_work_ptr &work) :
			db_(db), period_(period), tid_(id), work_(work) {}
		Task(db_handler_ptr db, size_t period, size_t id, const task_work_ptr &work, 
			string name) :	Task(db, period, id, work) { tname_ = name; }
		~Task() noexcept;
		/**
			@return [task_work_ptr work]	work/task function pointers 
		*/
		task_work_ptr get_work();	
		
		// task handlers
		size_t get_task_id();
		size_t get_period();
		virtual void start();
		virtual void stop();
		virtual void pause();
		virtual void resume();
		virtual void update(size_t new_period);
		virtual void run();
	};

	/*
		\description The Task Scheduler class, maintaining all tasks and their releated 
		resources, in the form of shared_ptr container. The class also contains a thread
		to receive/exec commands from outside
	*/
	class TaskScheduler : public Thread {
		unordered_map<size_t, task_container_ptr> task_pool_;	/* container including all tasks */
		vector<task_container_ptr> dyn_task_pool_;				/* newed~~/updated~~ task pool */
		static task_scheduler_ptr scheduler_;					/* singleton instance */
		db_handler_ptr db_;										/* DB handler for threads */
		size_t task_counter{ 0 };								/* assigned uid for each new task 
																will be unchanged after update task */
		/* for dyn_task_pool_ */
		atomic<bool> updated_{ true };							
		mutex mu_dpool_;
		condition_variable cv_dpool_;

		TaskScheduler(){}
		TaskScheduler(const TaskScheduler &) = delete;
		TaskScheduler(const TaskScheduler&&) = delete;
		TaskScheduler & operator = (const TaskScheduler&)  = delete;
		TaskScheduler & operator = (const TaskScheduler&&) = delete;
		
		using super = Thread;
	public:		
		~TaskScheduler() {}
		/**
			get static instance 
		*/
		static task_scheduler_ptr get() {
			if (scheduler_ == nullptr) {
				scheduler_ = task_scheduler_ptr(new TaskScheduler());
			}
			return scheduler_;
		}
		/**
			prepare for any resources needed, such as db connection, which will be kept open
		*/
		bool setup_context();
		/**
			add a new task to scheduler with running period and related working function pointer
			
			@param size_t period		task period
			@param task_work_ptr &work	function pointer to be run
			@param desc					name/description of the task
			@return size_t				return id of new created task; 0 if failed, o.w. > 0
		*/
		size_t add_task(size_t period, task_work_ptr &work, string desc = "");
		/**
			update task period with given task id

			@param size_t new_period	period to be used for updating
			@param size_t tid			task uid
			@return bool				return true if succeed
		*/
		bool update_task(size_t new_period, size_t tid);

		/* override Thread start */
		virtual void start();

		/**
			stop the task with specified task id
			
			@param size_t tid			task uid
		*/
		void cancel_task(size_t tid);
		
		/**
			stop all tasks in `task_pool_`
		*/
		void cancel_all();
		/**
			pause/resume specified task
			@param size_t tid			task id to be paused/resumed
		*/
		void pause_task(size_t tid);

		void resume_task(size_t tid);
		
		/**
			working context entry point
		*/
		virtual void run();

		/**
			release all contained resources
		*/
		void release_context();
	};
}

#endif
