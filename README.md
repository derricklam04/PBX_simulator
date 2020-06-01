# telephone_simulator
Simulation of PBX(Private Branch Exchange) network implemented in C. Features multithreading for simultaneous actions between different extensions.
Allow users to create/end extensions, dial/pickup/end calls, and send messages

## Installation
```make clean debug```

## Usage

To set up server socket:
```bin/pbx -p <portnumber>```

To create new extension (open new terminal):
```telnet localhost <portnumber>```

Commands within extensions:
```
pickup
hangup 
dial <extensionnumber> 
chat <msg> # sends a message to its currently connected extension
```

## Built With
<ul><li>CSAPP Library (csapp.h)</li></ul>
