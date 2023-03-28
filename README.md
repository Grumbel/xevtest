X11 Event Tester
================

Simple X11 event tester, similar to the `xev` tool, but focused on input
events and outputting them into one-line-per-event format.

Example
-------

    $ xevtest
    unhandled 12
    key-release      x=-351    y=222     state=0000000000010000  keycode=36
    motion-notify    x=9       y=174     state=0000000000010000
    motion-notify    x=33      y=171     state=0000000000010000
    motion-notify    x=62      y=168     state=0000000000010000
    motion-notify    x=81      y=166     state=0000000000010000
    motion-notify    x=93      y=166     state=0000000000010000
    motion-notify    x=104     y=164     state=0000000000010000
    motion-notify    x=114     y=164     state=0000000000010000
    ...

