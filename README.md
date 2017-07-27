hash_str2file
==================

A simple tools for add/delete a strings to a hash table.

#Function
A simple tools for add/delete a strings to/from a hash-table, also  
you can commit the hash-table to a file. the strings can be the  
formats like "string1=string2". the tools include a client command  
'hs' and a server monitor 'hms_monitor'. you should start hms_monitor  
first, eg: ./hms_monitor & then you can use command 'hs' to add/delete  
the strings to hash-table.  
![Image](https://raw.github.com/Janathan/hash_str2file/master/block_diagram.png)      

#Usage
**hmsg_monitor/hs tools**  
**hmsg_monitor** --start hash message monitor  
**hs [add/del] [name=value]** --add name and value to hash table  
**hs commit** --save name and value to file  
**hs get [name]** --get value of name  

develop test
