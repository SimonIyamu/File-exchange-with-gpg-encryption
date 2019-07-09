# File-exchange-with-gpg-encryption
Synchronize the hierarchy of files and directories of each client.  

### mirror_client  
Compilation: make  
Execution: ./mirror_client -n id -c common_dir -i input_dir -m mirror_dir -b buffer_size -l log_file  
where:
- id: a number to identify this certain client
- common_dir: is a directory which is used for the communication of the mirror_clients
- input_dir: is a directory that contains the files that this client wants to share
- b: the size of the pipe buffer 
- mirror_dir: is the directory where this client will store the files of the rest mirror_clients that participate in the file exchange.
- log_file: is a file where are the runtime messages are writen.

Example:  
./mirror_client -n 1 -c ./common -i ./1_input -m ./1_mirror -b 100 -l log_file1  
./mirror_client -n 2 -c ./common -i ./2_input -m ./2_mirror -b 100 -l log_file2  

### create_infiles.sh   
Creates a hierarchy of files and directories.  
Execution: ./create_infiles.sh dir_name num_of_files num_of_dirs levels  

### get_stats.sh  
Execution: cat logfile1 logfile2 ... logfilen | ./get_stats.sh   
