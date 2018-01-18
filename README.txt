============================================================================================================================================
=== I. Overview
============================================================================================================================================

The Clavis user-level scheduler is designed to implement various scheduling algortihms that 
we came up with in our SFU systems lab on the user level of the Linux OS. The scheduling 
algorithms work by periodically making a scheduling decision based on the workload info 
provided in the input file and/or online monitoring of the workload missrate. Then this 
scheduling decision is enforced by binding threads to cores. There is also a possibilty 
of running the workload with default Linux scheduler and to perform a simple binding 
threads to cores.

Very important! Please read (at the very least browse through) the following short paper 
before using Clavis: http://clavis.sourceforge.net/files/blagodurov-clavis.pdf
The paper will explain the purpose of our scheduler and briefly how it works.





============================================================================================================================================
=== II. How to install
============================================================================================================================================

1) Here is how to install packages necessary for Clavis:
# on CentOS:
yum install numactl-devel gcc-c++ ncurses-devel libpcap-devel elfutils-devel newt-devel binutils-devel zlib-static
# on Ubuntu:
apt-get install libnuma-dev elfutils libnewt-dev binutils-dev zlib-bin libelf-dev libnuma1 numactl iotop

---------------------------------------------------

2) Clavis accesses Model Specific Registers (MSR) in its work. Debian and other Linux distributions have the msr-tools package 
which basically contains two utilities: RDMSR for reading and WRMSR for writing. To install:
apt-get install msr-tools
Or you can download msr tools from here: http://www.kernel.org/pub/linux/utils/cpu/msr-tools/

The above user-space utilities depend upon the "msr" kernel module. Make sure that your Linux kernel has the "msr" module compiled:
cat /boot/config-2.6.32-5-openvz-amd64 | grep MSR
CONFIG_X86_MSR=m (or y)
If your CONFIG_X86_MSR is not set to either "m" or "y", then you need to re-compile your Linux kernel after setting it to “y” or “m”.

Make sure that the msr module is loaded upon boot:
modprobe msr
To check if its loaded:
ls /dev/cpu/0/msr
More info on msr: http://linux.koolsolutions.com/2009/09/19/howto-using-cpu-msr-tools-rdmsrwrmsr-in-debian-linux/

---------------------------------------------------

3) place the provided .toprc file into the same directory from where you run scheduler.out. The file 
makes sure that the top command will output only the fields necessary for the scheduler. You can check 
if it works by issuing top from the directory and then from your ~. The results should differ.

---------------------------------------------------

4) Describing workload in the input runfile.txt is currently not mandatory. Clavis is able to detect 
the workload processes on-the-fly (please see below). However, if you want to simply start the workload, 
not schedule it, you can still use runfile. In that case, you need to change runfile.txt like so:
fluidanimate 15 ./fluidanimate 16 5000 ./in_500K.fluid ./out.fluid
***rundir /home/storage2/fluidtest/fluidanimate/
***thread 0 [10]
fluidanimate2 20 ./fluidanimate 16 5000 ./in_500K.fluid ./out.fluid
***rundir /home/storage2/fluidtest/fluidanimate2/
***thread 0 [20]

The format is: <title of the program that will be used in the logs> \
<starting time of the program in seconds since the start of the user-level scheduler> \
<invocation string of the program binary, no intermediate shell scripts here>
        ***rundir <run directory>
        ***thread 0 [<signature for the first thread>]
          ...       
        ***thread N [<signature for the last thread>]

---------------------------------------------------

5) the scheduler invocation: ./scheduler.out d none Xeon_X5650_HT run.txt mt once fixed 0
will allow you to only start applications, but not schedule them.

To schedule, please install perf or pfmon on your system. Perf is the latest Linux kernel 
tool to track counters and is preferable, but works only on kernels above 2.6.29. 
To install on any version above 2.6.29:
# download and extract the source of kernel 2.6.34 (please use this exact version, 
# it is only for Clavis modified perf compilation, you can remove it afterwards)
wget http://www.kernel.org/pub/linux/kernel/v2.6/longterm/v2.6.34/linux-2.6.34.12.tar.bz2
bunzip2 linux-2.6.34.12.tar.bz2
tar -xf linux-2.6.34.12.tar
# download and extract Clavis modified perf from http://sourceforge.net/projects/clavis/files/
tar -xzf perf-mod-for-2.6.34.tar.gz
# replace the existing perf source, compile and (re)install:
rm -rf linux-2.6.34.12/tools/perf/
cp -pr perf linux-2.6.34.12/tools/
cd linux-2.6.34.12/tools/perf/
HOME="/usr"
make
make install

After that, please run the scheduler as follows: 
./scheduler.out dino perf Xeon_X5650_HT runfile.txt mt once fixed 0
Also, start the applications regularly via bash. Clavis will be able to detect them on-the-fly.

---------------------------------------------------

6) See signal-handling.c for Clavis invocation options. The scheduler terminates once 
every program in the workload completed its execution at least three times (can be adjusted).

---------------------------------------------------
   
7) In case you will have additional questions (especially about internal code of the scheduler), 
please don't hesitate to email me directly at: sergey_blagodurov@sfu.ca





============================================================================================================================================
=== III. Getting help
============================================================================================================================================

Got a question? Found a bug? Please contact the author directly.
Please do the following to get a useful response and save time for both of us:

1) Please briefly describe what do you want to use Clavis for? What is the purpose of your experiments? Without understanding what you want to see, it is hard to recommend the best use of Clavis for your task. Also, please elaborate a little bit on the workload you are using in your tests (what apps, what is their CPU usage, missrate, etc.).

2) Please indicate what Clavis version you're using, attach its output and all the log files to your email.

3) If Clavis crashes on your system, i.e. if you see "Segmentation fault", do the following:
      a. Make sure you run scheduler.out from a folder located locally on your server, and not from an NFS directory, otherwise the core dump will be of zero size.
      b. Make sure to issue invocation fom the command line directly, and not from scheduler_script.sh otherwise the core dump may not be created, for example:
      HOME=`pwd`
      export HOME
      cd $HOME
      ./scheduler.out dino perf Opteron_6164 runfile4t8z.txt st once fixed 0
      c. Go the the rundir and issue the following:
         gdb scheduler.out core
      d. Type "where" in the gdb prompt. Provide the output of this command in your email.

4) Send your question to blagodurov@gmail.com, sergey_blagodurov@sfu.ca.


Hope this helps!
--Sergey
