#!/bin/sh

verbose=false

if [ "x$1" = "x-v" ]
then
	verbose=true
	out=/dev/stdout
	err=/dev/stderr
else
	out=/dev/null
	err=/dev/null
fi

pts=2
timeout=30
preservefs=n

echo_n () {
	# suns can't echo -n, and Mac OS X can't echo "x\c"
	# assume argument has no doublequotes
	awk 'BEGIN { printf("'"$*"'"); }' </dev/null
}

runbochs () {
	# Find the address of the kernel readline function,
	# which the kernel monitor uses to read commands interactively.
	brkaddr=`grep 'readline$' obj/kern/kernel.sym | sed -e's/ .*$//g'`
	#echo "brkaddr $brkaddr"

	# Run Bochs, setting a breakpoint at readline(),
	# and feeding in appropriate commands to run, then quit.
	t0=`date +%s.%N 2>/dev/null`
	(
		# The sleeps are necessary in some Bochs to 
		# make it parse each line separately.  Sleeping 
		# here sure beats waiting for the timeout.
		echo vbreak 0x8:0x$brkaddr
		sleep .5
		echo c
		# EOF will do just fine to quit.
	) | (
		ulimit -t $timeout
		# date
		bochs -q 'display_library: nogui' \
			'parport1: enabled=1, file="bochs.out"' 
		# date
	) >$out 2>$err
	t1=`date +%s.%N 2>/dev/null`
	time=`echo "scale=1; ($t1-$t0)/1" | sed 's/.N/.0/g' | bc 2>/dev/null`
	time="(${time}s)"
}


# Usage: runtest <tagname> <defs> <strings...>
runtest () {
	perl -e "print '$1: '"
	rm -f obj/kern/init.o obj/kern/kernel obj/kern/bochs.img 
	[ "$preservefs" = y ] || rm -f obj/fs/fs.img
	if $verbose
	then
		echo "gmake $2... "
	fi
	gmake $2 >$out
	if [ $? -ne 0 ]
	then
		echo gmake $2 failed 
		exit 1
	fi
	runbochs
	if [ ! -s bochs.out ]
	then
		echo 'no bochs.out'
	else
		shift
		shift
		continuetest "$@"
	fi
}

quicktest () {
	perl -e "print '$1: '"
	shift
	continuetest "$@"
}

stubtest () {
    perl -e "print qq|$1: OK $2\n|";
    shift
    score=`expr $pts + $score`
}

continuetest () {
	okay=yes

	not=false
	for i
	do
		if [ "x$i" = "x!" ]
		then
			not=true
		elif $not
		then
			if egrep "^$i\$" bochs.out >/dev/null
			then
				echo "got unexpected line '$i'"
				if $verbose
				then
					exit 1
				fi
				okay=no
			fi
			not=false
		else
			egrep "^$i\$" bochs.out >/dev/null
			if [ $? -ne 0 ]
			then
				echo "missing '$i'"
				if $verbose
				then
					exit 1
				fi
				okay=no
			fi
			not=false
		fi
	done
	if [ "$okay" = "yes" ]
	then
		score=`expr $pts + $score`
		echo OK $time
	else
		echo WRONG $time
	fi
}

# Usage: runtest1 [-tag <tagname>] <progname> [-Ddef...] STRINGS...
runtest1 () {
	if [ $1 = -tag ]
	then
		shift
		tag=$1
		prog=$2
		shift
		shift
	else
		tag=$1
		prog=$1
		shift
	fi
	runtest1_defs=
	while expr "x$1" : 'x-D.*' >/dev/null; do
		runtest1_defs="DEFS+='$1' $runtest1_defs"
		shift
	done
	runtest "$tag" "DEFS='-DTEST=_binary_obj_user_${prog}_start' DEFS+='-DTESTSIZE=_binary_obj_user_${prog}_size' $runtest1_defs" "$@"
}

qemu_test_httpd() {
	pts=5

	echo ""

	perl -e "print '    wget localhost:8080/: '"
	if wget -o wget.log -O /dev/null localhost:8080/; then
		echo "WRONG, got back data";
	else
		if egrep "ERROR 404" wget.log >/dev/null; then
			score=`expr $pts + $score`
			echo "OK";
		else
			echo "WRONG, did not get 404 error";
		fi
	fi

	perl -e "print '    wget localhost:8080/index.html: '"
	if wget -o /dev/null -O qemu.out localhost:8080/index.html; then
		if diff qemu.out fs/index.html > /dev/null; then
			score=`expr $pts + $score`
			echo "OK";
		else
			echo "WRONG, returned data does not match index.html";
		fi
	else
		echo "WRONG, got error";
	fi

	perl -e "print '    wget localhost:8080/random_file.txt: '"
	if wget -o wget.log -O /dev/null localhost:8080/random_file.txt; then
		echo "WRONG, got back data";
	else
		if egrep "ERROR 404" wget.log >/dev/null; then
			score=`expr $pts + $score`
			echo "OK";
		else
			echo "WRONG, did not get 404 error";
		fi
	fi

	kill $qemu_pid
	wait

	t1=`date +%s.%N 2>/dev/null`
	time=`echo "scale=1; ($t1-$t0)/1" | sed 's/.N/.0/g' | bc 2>/dev/null`
	time="(${time}s)"
}

qemu_test_tcpsrv() {
	str="$t0: network server works"
	echo $str | nc -q 3 localhost 4242 > qemu.out

	kill $qemu_pid
	wait

	t1=`date +%s.%N 2>/dev/null`
	time=`echo "scale=1; ($t1-$t0)/1" | sed 's/.N/.0/g' | bc 2>/dev/null`
	time="(${time}s)"

	if egrep "^$str\$" qemu.out > /dev/null
	then
		score=`expr $pts + $score`
		echo OK $time
	else
		echo WRONG $time
	fi
}

# Usage: runqemu <tagname> <defs> <strings...>
runqemu() {
	perl -e "print '$1: '"
	rm -f obj/kern/init.o obj/kern/kernel obj/kern/bochs.img 
	[ "$preservefs" = y ] || rm -f obj/fs/fs.img
	if $verbose
	then
		echo "gmake $2... "
	fi
	gmake $2 >$out
	if [ $? -ne 0 ]
	then
		echo gmake $2 failed 
		exit 1
	fi

	t0=`date +%s.%N 2>/dev/null`
	qemu -hda obj/kern/bochs.img -hdb obj/fs/fs.img \
	     -net user -net nic,model=i82559er -parallel /dev/stdout \
	     -redir tcp:4242::10000 -redir tcp:8080::80 \
	     -nographic -pidfile qemu.pid -pcap slirp.cap 2>/dev/null&

	sleep 3 # wait for qemu to start up

	qemu_pid=`cat qemu.pid`
	rm -f qemu.pid
}

# Usage: runtestq [-tag <tagname>] <progname> [-Ddef...] STRINGS...
runtestq() {
	if [ $1 = -tag ]
	then
		shift
		tag=$1
		prog=$2
		shift
		shift
	else
		tag=$1
		prog=$1
		shift
	fi
	testnet_defs=
	while expr "x$1" : 'x-D.*' >/dev/null; do
		testnet_defs="DEFS+='$1' $testnet_defs"
		shift
	done
	runqemu "$tag" "DEFS='-DTEST=_binary_obj_user_${prog}_start' DEFS+='-DTESTSIZE=_binary_obj_user_${prog}_size' $testnet_defs" "$@"

	# now run the actuall test
	qemu_test_${prog}
}

score=0

# Reset the file system to its original, pristine state
resetfs() {
	rm -f obj/fs/fs.img
	gmake obj/fs/fs.img >$out
}


resetfs

runtest1 -tag 'fs i/o [testfsipc]' testfsipc \
	'FS can do I/O' \
	! 'idle loop can do I/O' \

quicktest 'read_block [testfsipc]' \
	'superblock is good' \

quicktest 'write_block [testfsipc]' \
	'write_block is good' \

quicktest 'read_bitmap [testfsipc]' \
	'read_bitmap is good' \

quicktest 'alloc_block [testfsipc]' \
	'alloc_block is good' \

quicktest 'file_open [testfsipc]' \
	'file_open is good' \

quicktest 'file_get_block [testfsipc]' \
	'file_get_block is good' \

quicktest 'file_truncate [testfsipc]' \
	'file_truncate is good' \

quicktest 'file_flush [testfsipc]' \
	'file_flush is good' \

quicktest 'file rewrite [testfsipc]' \
	'file rewrite is good' \

quicktest 'serv_* [testfsipc]' \
	'serve_open is good' \
	'serve_map is good' \
	'serve_close is good' \
	'stale fileid is good' \

pts=5
runtest1 -tag 'motd display [writemotd]' writemotd \
	'OLD MOTD' \
	'This is /motd, the message of the day.' \
	'NEW MOTD' \
	'This is the NEW message of the day!' \

preservefs=y
runtest1 -tag 'motd change [writemotd]' writemotd \
	'OLD MOTD' \
	'This is the NEW message of the day!' \
	'NEW MOTD' \
	! 'This is /motd, the message of the day.' \

pts=8
preservefs=n
runtest1 -tag 'spawn via icode [icode]' icode \
	'icode: read /motd' \
	'This is /motd, the message of the day.' \
	'icode: spawn /init' \
	'init: running' \
	'init: data seems okay' \
	'icode: exiting' \
	'init: bss seems okay' \
	"init: args: 'init' 'initarg1' 'initarg2'" \
	'init: exiting' \

echo PART A SCORE: $score/40

partascore=$score


score=0
pts=45
preservefs=y
runtestq -tag 'tcp echo server [tcpsrv]' tcpsrv

#pts=15 # points are allocated in the test code
preservefs=y
runtestq -tag 'web server [httpd]' httpd

echo PART B SCORE: $score/60

partbscore=$score

if [ $partascore -lt 40 -o $partbscore -lt 60 ]; then
    exit 1
fi

