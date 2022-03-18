MonoMux
=======

MonoMux (for _Monophone Terminal Multiplexer_ &mdash; pun intended) is a system tool that allows executing terminal sessions in the background with on-demand attaching to them.
MonoMux is a tool similar to [`screen`](http://gnu.org/software/screen/) and [`tmux`](https://github.com/tmux/tmux/wiki).
It allows most of the core features of _screen_ or _tmux_, with being less intrusive about its behaviour when it comes to using these tools with modern *terminal emulators*.

> **⚠️ Warning!** Currently, _MonoMux_ is designed with only supporting Linux operating systems in mind.
> Most of the project is written with POSIX system calls in mind, but there are *some* GNU extensions used.

> **⚠️ Note:** _MonoMux_ is **NOT** a terminal emulator by itself!
> To use it, you may use any of your favourite terminal emulators.

Dependencies
------------

MonoMux uses modern C++ features, and as such, a C++17-capable compiler and associated standard library is needed to compile and execute the tool.
There are no other dependencies.

Installation
------------

> TODO.

Usage
-----

> TODO.

Why?
----

One of the most important contexts where _screen_ or _tmux_ comes to mind is over remote sessions.
If a remote connection breaks &mdash; or the local graphical terminal closes or crashes &mdash;, the terminal session behind the connection is sent a **`SIGHUP`** signal, for which most programs exit.
This results in the loss of shell history, and the interrupt of running programs.

The most crucial problem from an interactive work's point-of-view with existing tools is that both _screen_ and _tmux_ **act as terminal emulators** themselves.
Their behaviour is to _parse_ the [VT sequences](http://vt100.net/docs/vt100-ug/chapter3.html) of the output received from the "remote" terminal and emit them to the attached client(s).
Programs using extensive modern, or terminal specific features will have to fall back to the older and more restrictive set of what _screen_ or _tmux_ understands.

A fork of _screen_, [`dtach`](http://github.com/crigler/dtach) was created which emulates **only** the attach/detach features of _screen_.
However, _dtach_ has a straightforward and non-trivial interface, e.g. the user must specify the connection socket file manually.

(Moreover, all of the aforementioned tools are written in C.)

MonoMux aims to combine the good aspects of all of these tools but remove almost all of the possible hurdles in the way of tools running in the background session.

 * Attach/detach features are supported without the need of parsing control sequences.
 * Better defaults than _dtach_: no need to specify an exit escape sequence or modern resize events.
 * Like _tmux_, there is meaningful session management by default, giving an interactive attach menu if multiple sessions exist.

Written in C++17, with object-oriented design in mind.
This might result in a larger binary than for other tools, however, _MonoMux_ is intended for user systems, not embedded contexts.
