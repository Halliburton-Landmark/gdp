
Configuration 
-----------------------------------
Currently, as default, it is assumed that Key Distrribution service and registration service run on the same system. So they can use the same certificate and secret key. 


The parameters for key distribution serivce are located in the following file. 
/etc/ep_adm_param/keydistd 

To run Key distribution service, the following parameters need to be set. 





For devices to interact with the key distribution service,   
each device or service has to set the parameter for 
