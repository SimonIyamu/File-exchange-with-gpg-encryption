# File-exchange-with-gpg-encryption
Synchronize the hierarchy of files and directories of each client.  

- mirror_client  
Compilation: make  
Execution: ./mirror_client -n id -c common_dir -i input_dir -m mirror_dir -b buffer_size -l log_file  
Example:  
./mirror_client -n 1 -c ./common -i ./1_input -m ./1_mirror -b 100 -l log_file1  
./mirror_client -n 2 -c ./common -i ./2_input -m ./2_mirror -b 100 -l log_file2  

- create_infiles.sh   
Creates a hierarchy of files and directories.  
Execution: ./create_infiles.sh dir_name num_of_files num_of_dirs levels  

- get_stats.sh  
Execution: cat logfile1 logfile2 ... logfilen | ./get_stats.sh   
