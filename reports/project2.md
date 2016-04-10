Final Report for Project 2: User Programs
=========================================

Student test repost:
My_test_1:
           Read from the the stdin and write the message we just read in. the output of write should be the same as the message read takes.         
Kernel bug:    
           - if the file descriptor is not validated before using it, the file might not be able to be opened or modified or random memory might be accessed.
           - If the buffer size is too large and goes beyond the user stack, kernel memory might be modified and the output of write would be random thing.
      2. My_test_2: create a file with file_name goes beyond the user space. Then exec a child process trying to open (some operation) on a different file, say example.txt. 
          Before the file is created, the file_name is checked if it is mapped and within the user space. The create function should return -1 because the file_name is not valid. If the lock is not released properly when create function fails, the following child process is not able to do any operations on a different file and stays there forever waiting for the lock, which is not supposed to happen. 
         Kernel bug:
          - if the file operations does not check if the passed in file_name is not checked if it is mapped and within the user space, the create function would not cause any error
          -  

