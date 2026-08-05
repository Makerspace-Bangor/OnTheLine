# OnTheLine
<pre>
HMI_decode/
├── ard_hmi.pdf              <-- initial docs
├── ard_hmi.tex              <-- initial docs src
├── arduino_hmi_listener.png <-- output
├── hmi_listener
│   └── hmi_listener.ino     <-- logs register requests from arduino minima + W5500 shield
├── hmi_register_logger2.py  <-- logs register requests from a PC
├── hmi_registers.txt        <-- Example PC output
├── MiSmTCP.py               <-- supporting library

</pre>

The Gist:
Read the requests, and return 0. this will keep the HMI active
