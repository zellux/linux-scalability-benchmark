Linux scalability benchmark
====

Some simple benchmarks for evaluating Linux scalability on many-cores.

### dirmaker
Create numerous directories under /tmp/dir.

### sockbench
Create a socket listener on each ethernet interface, and connect to them repeatedly.

### mmapbench
Create multiple threads which touch a large mmaped file one by one.

### sembench
Ping-pong test for semephore scalabity on many-cores.

### lockstat
Modified version of lockstat tool in lockmeter (http://oss.sgi.com/projects/lockmeter/).
