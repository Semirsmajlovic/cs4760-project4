Semir Smajlovic
Project 4

## Instructions:
1. Run 'make' to compile all the files.
2. Execute './oss -h' to view the help instructions.
3. Run './oss' to start the execution process.

## Details:
- The output will be written to a file called output_log.txt.

## Known issues:
- There aren't any major issues that I know about, I did totally forget to add in the feature to specify a particular name for the log file because I just got back from Spring Break and have other assignments, but I have added the max seconds and help options.


UPDATE: 
- Looks like for whatever reason, the process executes properly on my machine, but it will not execute properly on CS OPSYS. I just got back from my flight arrival 3 hours ago and did not have the resources to login and build the project using OPSYS since I was using a borrowed laptop on Spring Break. I have attached a video (https://streamable.com/uhpfjb) of the execution on my machine to show that it is indeed working, I can't figure out why it's not properly executing on OPSYS.

----------------------------------

### Purpose:
The goal of this homework is to learn about process scheduling inside an operating system. You will work on the specified
scheduling algorithm and simulate its performance.

### Task:
In this project, you will simulate the process scheduling part of an operating system. You will implement time-based
scheduling, ignoring almost every other aspect of the os. In particular, this project will involve a main executable managing
the execution of concurrent user processes just like a scheduler/dispatcher does in an os. You may use message queues or
semaphores for synchronization, depending on your choice.