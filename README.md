Plugins to interface exotic hardware with SIMH.

### ding

Rings a bell when a non-NUL character is received on stdin.  Uses
HIDAPI to talk to Aaron Patterson's
[*analog-terminal-bell*](https://github.com/tenderlove/analog-terminal-bell).

### midi

Listens to events from MIDI contollers.  Sends the values as analog
inputs to SIMH.


### template

Template plugin. Does nothing but logs input from SIMH.

Attach arguments: log flags (0=none), address and port (ignored).

```
ATTACH PANEL template-panel 1 127.0.0.1 31962
```
