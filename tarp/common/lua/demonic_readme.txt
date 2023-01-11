NOTE:
---> see the 'code_examples/lua' repo/dir to find all the files mentioned below;
I've kept here only the library and the header file itself.

Compile with:
    gcc -shared -fPIC demonic.c -Ilua-5.3.0/src/ -o demonic.so
This is assuming you have the lua5.3 sources cloned locally in the specified dir.

the daemonized_lua_script_example.lua script daemonizes itself through the demonic.so 
shared C library; 
check the contents of this script to see what it does;


* * *
I was wondering why the script, even though daemonized, didn't adopt parent pid of 1 (i.e.
why init didn't adopt it as its child');
the ppid of my process was 7919.

But on running ps:

└─$ ps -p 7919
    PID TTY          TIME CMD
   7919 ?        00:00:03 systemd

things suddenly become clear: systemd -- which is the initd daemon on modern systems, has this pid. 
So initd HAD adopted my process as its child. it's just that the de-facto initd process doesn't have PID 
1.


* ** 
the lua_daemon.lua script was an older attempt of mine at daemonizing a lua script;
this makes use of the exoisting luaposix libraries rather than using my own C lib.
this works fine but luaposix doesn't make available ALL of the posix calls. For example,
it's missing setsid() which is normally part of the call sequence for daemonizing a process.
MY module otoh provides that as well -- AND it has the advantage of being easily 
modifiable since I wrote it.



