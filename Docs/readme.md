The alure, and promise of the Uno Q, for me is increased prgram space.
Unfortunately, this came at the expense of decreased performance. 
Im not saying I need ms response times, but you know, 10ms? 50ms? 100?
any of those would work. in practice I was seeing much less, and the latency would cause the HMI 
to loose its connectivity / display error messages, which is what I do no want.

So then I explored a bunch of other hardware, and settled on the Ardio Minima.
its specs are the same as the Arduino Mega, but with the UNO formfactor. 
There is also an ESP32 board in the UNO form factor that works well, with signifigantly better specs.

The PLCs, and HMI are all ethernet, so I have to be able to use the ethernet shield, 
and have it perform, at least reasonably well. The issue with the Arduino UNO in my 
expiraments is not performative, just developing code that is within the limited memory spec
was the real challenge.  
