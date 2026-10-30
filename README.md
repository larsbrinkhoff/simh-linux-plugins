Plugins to interface exotic hardware with SIMH.

### ding

Rings a bell when a non-NUL character is received on stdin.  Uses
HIDAPI to talk to Aaron Patterson's
[*analog-terminal-bell*](https://github.com/tenderlove/analog-terminal-bell).

### midi

Listens to events from MIDI contollers.  Sends the values as analog
inputs to SIMH.
