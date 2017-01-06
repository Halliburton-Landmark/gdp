# A CoAP=>GDP gateway for TI I3 motes

How to use:

1. Acquire TI hardware/software and set it up.
2. Ensure GDP client side is installed on the BeagleBone Black.
3. Acquire libcoap (https://libcoap.net/) and compile it for BBB. You should
   get a `coap-client` binary in `.../examples`, or somewhere in your system
   path (based on how far have you gone with the installation process).
4. If you do not have the required logs created, the next step will fail.
   Create the required logs.
5. Run as follows:

```
$ ./restart-wrapper ./CoAP-gateway.py <path to coap-client> <prefix>
```


## More information

* `restart-wrapper` is a shell script; it treats the rest of the arguments
  as the shell command that gets restarted every 20 minutes.
* `CoAP-gateway.py` is a python script that fetches the nodes, starts 2 threads
  per node (one thread launches `coap-client`, the other thread reads the 
  information and pushes to the GDP).
