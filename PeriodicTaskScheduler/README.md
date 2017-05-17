
demo for PeriodicTaskScheduler

Tasks:
==============
the demo lasts 20 seconds in total;  
at the beginning only task1 and task2 are running;  
at 7th second, task1 will be destroyed;  
at 11th sec task3 will be added.  
at every 5th second a task will be selected randomly
and its period will be updated with a random number(second)

DB access:
=============
each result will be labeled with task id, and will be
identified with the same task id for  the next time.  
there're 5 types of value: raw value, min/max/average;  
to estimate average value, the last inserted (the most recent)
record will be retrieved and the average value will be
updated using online average method for simplicity:  
	mean_n = mean_n-1 + (x_n - mean_n-1)/n


Development environment:
================
compiled and tested with  
Windows 10 (64bit) Visual Studio 2017
