Final Report for Project 1: Threads
===================================

We changed almost nothing from our original design. These were the only changes:
- For task 2, we forgot to include a pointer in the thread struct to reference the lock that the thread is waiting on. This was not hard to add.
- For task 3, we needed to include a counter for the number of "ready threads", which we overlooked in our design. We ran into a few problems while trying to add the code for this, due to some quirks of the idle thread implementation.
- We also added comparator functions, which we didn't list in the design doc because we figured it was boilerplate.
- Finally, in task 2, `thread_get_priority()` was made to return the effective priority instead of the baseline priority, due to a change in the spec.

I (Gavin Song) wrote the design document, pushed most of the code, and did most of the GDB debugging. This was mostly a result of my being the most familiar with the specific details I worked out while finalizing the design doc. However, the designing and debugging effort as a whole was a collaborative effort in which every team member contributed. There were four team meetings where everybody showed up and worked together on solutions for hours. We bounced ideas off of each other and helped each other through technical problems such as with git. Beyond that, it's hard to say who had what roles. It wouldn't be too far off to say that we were programming as a group.

I (Jun Tan) provided the draft solution of the design document additional questions, and added addtional variable and function declarations which is the result of I recorded down the coing details as we designed the project. The credit of designing and debugging should be given to every team member because we worked togethter to solve the issues. The lesson I leant from this project is that we should spend less time on the design part. Even though we finished on time, we spent too much time on designing which is about two third of the amount of time we spent on the whole project. We are fortunate that we are able to discover the bug and fix it very quick this time. 
