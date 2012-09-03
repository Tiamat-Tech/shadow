This section discusses the format of Shadow's log files, and how to analyze the results contained therein.

## the log file

Shadow produces log messages in the following format:

```
real-time [thread-id] virtual-time [logdomain-loglevel] [hostname-ip] [function-name] MESSAGE
```

+ _real-time_:  
the wall clock time since the start of the experiment, represented as `hours:minutes:seconds:nanoseconds`
+ _thread-id_:  
the ID of the worker thread that generated the message
+ _virtual-time_:  
the simulated time since the start of the experiment, represented as `hours:minutes:seconds:nanoseconds`
+ _logdomain_:  
either `shadow` or the name of one of the plug-ins as specified in the _id_ tag of the _plugin_ element in the XML file (e.g., `scallion`, `filetransfer`, `echoplugin`, `torrent`, `browser`)
+ _loglevel_:  
one of `error` < `critical` < `warning` < `message` < `info` < `debug`, in that order
+ _hostname_:  
the name of the node as specified in the _id_ tag of the _node_ element in the XML file
+ _ip_:  
the IP address of the node as specified in the _ip_ tag of the _node_ element in the XML file, or a random IP address if one is not specified  
**NOTE**: _all IP addresses are random until [this feature](https://github.com/shadow/shadow/issues/39) is completed_
+ _function-name_:  
the name of the function logging the message
+ _MESSAGE_:  
the actual message to be logged

By default, Shadow only prints plug-in and core messages at or below the `message` log level. This behavior can be changed using the Shadow option `-l` or `--log-level`.  
**NOTE**: _in addition to Shadow's log level, Scallion experiments should also change Tor's log level in the *torrc files if lower level Tor messages are desired_

## gathering statistics

Shadow includes a heartbeat message that contains some useful system information for each virtual node in the experiment. This information is contained in messages containing the string `shadow-heartbeat` and includes:

+ CPU _value_ %:  
the percentage of time spent executing code _inside_ the plug-in over the previous interval  
**NOTE**: _this value will be 0 if using `--cpu-threshold=-1`__
+ MEM _value_ KiB:  
the total memory currently consumed by the node's plug-in, in Kibibytes
+ interval _value_ seconds:  
the number of seconds used to calculate interval statistics
+ alloc _value_ KiB:  
the amount of memory allocated (i.e. malloced) in the last interval, in Kibibytes
+ dealloc _value_ KiB:  
the amount of memory de-allocated (i.e. freed) in the last interval, in Kibibytes
+ Rx _value_ B:  
the amount of network data received in the last interval, in Bytes
+ Tx _value_ B:  
the amount of network data sent in the last interval, in Bytes

These heartbeats are logged at the `message` level every `60` seconds by default.  The heartbeat log level can be changed with the option `-g` or `--stat-log-level` and the heartbeat interval set with the option `-h` or `--stat-interval`.

Each plug-in also generally prints useful statistics, such as file download size and timing information. See the plug-in usage pages for an explanation of what each provides.

## parsing and plotting results

Shadow includes a python script `contrib/analyze.py` that can parse a log file and extract these important statistics. This script can also plot the results using python's pylab module.

The script is meant to be generic enough to parse any Shadow output and extract whatever information it knows about. This includes network throughput over time, client download statistics, and client load statistics (some of these are gathered from plug-in log messages).

For more information:
```bash
python contrib/analyze.py --help
python contrib/analyze.py parse --help
python contrib/analyze.py plot --help
```

**NOTE**: _the analyze.py script requires some python modules, most notably the `pylab` module_

As a quick example of how to use the script, consider a set of experiments where we would like to analyze the effect of changing the size of our nodes' network interface receive buffer. We run the following 3 experiments:

```bash
shadow --tcp-windows=1 --file > window1.log
shadow --tcp-windows=1000 --file > window1000.log
```

To parse these log files, we use the `contrib/analyze.py` script as follows:

```bash
python contrib/analyze.py parse --cutoff=0 --output=window1 window1.log
python contrib/analyze.py parse --cutoff=0 --output=window1000 window1000.log
```

Each of the directories `window1/` and `window1000/` now contain data statistics files extracted from the log files. We can now combine and visualize these results by plotting them with pylab:

```bash
python contrib/analyze.py plot --title "Shadow TCP Window Test" --prefix "window" --data window1/ "1 packet" --data window1000/ "1000 packets"
```

See any of the graphs in `./graphs`, or if you have `pdftk` installed, you can simply view the `window-combined.pdf` file.